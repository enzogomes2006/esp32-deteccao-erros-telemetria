#include "Arduino.h"
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
int64_t fakeClock=1000;
Port Serial,Serial2;
#include "../firmware/rx/rx.ino"
#include "../firmware/noise/noise_targets.h"
#include "../firmware/noise/capture_schedule.h"
using Body=std::array<uint8_t,8>;
Body body(uint16_t seq,bool crc=false) {
  Body b={7,uint8_t(seq),uint8_t(seq>>8),0xE5,0xAC,0xB6,0x89,0};
  b[7]=crc?crc8(b.data(),7):uint8_t(0-sum8(b.data(),7)); return b;
}
void reset(unsigned count=10,bool crc=false,uint16_t origin=0,bool pause=false) {
  fakeClock=1000;Serial.log.clear();Serial.q.clear();Serial2.q.clear();
  Serial2.enabled=true;uartPaused=false;useCRC=crc;prepareRun(count,origin,pause);
}
void marker(uint32_t m){for(int sh=24;sh>=0;sh-=8)consume(uint8_t(m>>sh));}
void data(const Body& b,int64_t at) {
  assert(at>=fakeClock);fakeClock=at;marker(START_MARK);
  for(unsigned i=0;i<8;i++){fakeClock=at+10000+3000*i;consume(b[i]);}
}
void reference(const Body& b,int64_t at) {
  assert(at>=fakeClock);fakeClock=at;marker(DEBUG_MARK);for(auto c:b)consume(c);
  if(measuring && pauseRequested)pauseUART();
}
void frame(const Body& rx,const Body& tx,int64_t at){data(rx,at);reference(tx,at+45000);}
void checkComplete(unsigned ok,unsigned corrupt,unsigned lost,unsigned nd) {
  assert(!measuring && validMeasurement && classified()==wanted);
  assert(totals[0]==ok&&totals[1]==corrupt&&totals[2]==lost&&totals[3]==nd);
}
unsigned pop(uint8_t v){unsigned n=0;while(v){n+=v&1;v>>=1;}return n;}
void functionalTests() {
  const uint8_t known[]={'1','2','3','4','5','6','7','8','9'};
  assert(crc8(known,9)==0xF4);
  for(bool crc:{false,true}) {
    reset(10,crc);for(unsigned s=0;s<10;s++){auto b=body(s,crc);frame(b,b,1000000+s*101000);}checkComplete(10,0,0,0);
    // TODOS os 64 bits, incluindo LEN/seq/CHK: um erro simples e detectado.
    for(unsigned bit=0;bit<64;bit++) {
      reset(1,crc);auto tx=body(0,crc),rx=tx;rx[bit/8]^=1U<<(bit%8);
      data(rx,1000000);
      assert(linkState.expected==0 && !pending.delivered);
      assert(pending.decided && pending.checked==(bit>=8));
      reference(tx,1045000);
      if(bit<8)checkComplete(0,0,1,0);else checkComplete(0,1,0,0);
    }
  }
  // Header divergente em descartado e esperado, nao invalida auditoria.
  reset(3,true);auto tx=body(0,true),rx=tx;rx[1]^=0x80;
  data(rx,1000000);assert(linkState.expected==0 && linkState.anonymous==1);
  const auto before=linkState;reference(tx,1045000);
  assert(linkState.expected==before.expected&&linkState.anonymous==before.anonymous&&validMeasurement);
  tx=body(1,true);rx=tx;rx[2]^=0x80;frame(rx,tx,1101000);
  tx=body(2,true);frame(tx,tx,1202000);checkComplete(1,2,0,0);
  assert(linkState.missingEvidence==0&&linkState.ambiguities==0);
  // Descarte seguido de perda real: evidencia desconta o descarte.
  reset(4);tx=body(0);rx=tx;rx[1]^=2;frame(rx,tx,1000000);
  tx=body(2);frame(tx,tx,1202000);tx=body(3);frame(tx,tx,1303000);
  checkComplete(2,1,1,0);assert(linkState.missingEvidence==1);
  // LEN invalido e um PERDIDO, nunca C mais P da mesma posicao.
  reset(3);tx=body(0);rx=tx;rx[0]=6;frame(rx,tx,1000000);
  tx=body(1);rx=tx;rx[7]^=1;frame(rx,tx,1101000);
  tx=body(2);frame(tx,tx,1202000);checkComplete(1,1,1,0);assert(linkState.missingEvidence==0);
  // Colisao SUM8 altera payload: decisao nao muda ao receber verdade.
  for(bool crc:{false,true}) {
    reset(1,crc);tx=body(0,crc);rx=tx;rx[3]&=127;rx[4]&=127;
    data(rx,1000000);const auto state=linkState;const bool decision=pending.accepted;
    reference(tx,1045000);assert(linkState.expected==state.expected);
    if(crc){assert(!decision);checkComplete(0,1,0,0);}else{assert(decision);checkComplete(0,0,0,1);}
  }
  // Colisao SUM8 no header e payload: mostra sequencia errada do enlace.
  reset(129);tx=body(128);rx=tx;rx[1]&=127;rx[3]&=127;
  data(rx,1000000);assert(pending.accepted&&linkState.expected==1);
  reference(tx,1045000);assert(acceptedHeaderErrors==1&&results[128]==RESULT_UNDETECTED);
  assert(linkState.expected==1&&!validMeasurement&&!measuring); // janela incompleta, nao inventa 128 perdas
  // Mesmo caso no meio de uma janela com todos os eventos: tabela valida,
  // mas falha de sequenciamento e cabecalho ficam explicitas.
  reset(130);
  for(unsigned s=0;s<130;s++) {
    tx=body(s);rx=tx;if(s==128){rx[1]&=127;rx[3]&=127;}
    frame(rx,tx,1000000+s*101000);
  }
  checkComplete(129,0,0,1);assert(acceptedHeaderErrors==1&&linkState.expected==130);
  // Colisao apenas em seq/CHK: payload correto (OK), mas header diverge.
  reset(130);
  for(unsigned s=0;s<130;s++) {
    tx=body(s);rx=tx;
    if(s==128){assert(tx[7]&128);rx[1]&=127;rx[7]&=127;}
    frame(rx,tx,1000000+s*101000);
  }
  checkComplete(130,0,0,0);assert(acceptedHeaderErrors==1&&results[128]==RESULT_OK);
  // Duplicado/atrasado sem nova entrega nem nova linha de classificacao.
  reset(4);tx=body(0);frame(tx,tx,1000000);frame(tx,tx,1101000);
  assert(classified()==1&&duplicateAuditCount==1&&linkState.oldOrRepeated==1);
  tx=body(1);frame(tx,tx,1202000);tx=body(2);frame(tx,tx,1303000);
  // eventCount nao deve encerrar janela so porque houve duplicado.
  assert(measuring);tx=body(3);frame(tx,tx,1404000);checkComplete(4,0,0,0);
  // Sem copia: nao transforma quadro aceito em OK/perdido.
  reset(1);tx=body(0);data(tx,1000000);
  fakeClock=markerAt+FRAME_TIMEOUT_US+1;serviceTimers();
  assert(!validMeasurement&&classified()==0&&!measuring&&noReferenceCount==1);
  // Referencia orfa nao muda enlace nem inicia timer de perda.
  reset(2);reference(body(0),1000000);
  assert(!validMeasurement&&!streamStarted&&linkState.expected==0&&classified()==0);
  // Referencia invalida por residuo nao classifica nem corrige controle.
  reset(1);tx=body(0);data(tx,1000000);auto bad=tx;bad[7]^=1;reference(bad,1045000);
  assert(!validMeasurement&&classified()==0&&linkState.expected==1);
  // Corpo parcial expira: nova marca funciona, auditoria anterior e invalida.
  reset(2);fakeClock=1000000;marker(START_MARK);consume(7);
  fakeClock+=FRAME_TIMEOUT_US+1;serviceTimers();assert(parser==0&&!validMeasurement);
  tx=body(1);frame(tx,tx,1101000);assert(!measuring&&results[1]==RESULT_OK);
  // Dados iguais a marcadores dentro do corpo/copia nao abrem outro quadro.
  reset(1);tx=body(0);tx[3]=0xA5;tx[4]=0x5A;tx[5]=0xC3;tx[6]=0x3C;tx[7]=uint8_t(0-sum8(tx.data(),7));
  frame(tx,tx,1000000);checkComplete(1,0,0,0);assert(eventCount==1);
  reset(1);tx[3]=0xD5;tx[4]=0x6A;tx[5]=0xB4;tx[6]=0x4B;tx[7]=uint8_t(0-sum8(tx.data(),7));
  frame(tx,tx,1000000);checkComplete(1,0,0,0);
  // Wrap, incluindo descarte com seq recebida errada antes do zero.
  reset(4,false,65534);tx=body(65534);frame(tx,tx,1000000);
  tx=body(65535);rx=tx;rx[2]&=127;frame(rx,tx,1101000);assert(linkState.expected==65535);
  tx=body(0);frame(tx,tx,1202000);assert(linkState.expected==1);
  tx=body(1);frame(tx,tx,1303000);checkComplete(3,1,0,0);assert(linkState.missingEvidence==0);
  // Pausa de recuperacao: ausencia inferida por timeout/lacuna, sem copia.
  reset(10,true,0,true);tx=body(0,true);frame(tx,tx,1000000);
  tx=body(1,true);frame(tx,tx,1101000);assert(uartPaused&&results[1]==RESULT_OK);
  fakeClock=nextLossAt;serviceTimers();assert(linkState.missingEvidence==1);
  fakeClock=pausedAt+PAUSE_DURATION_US;serviceTimers();assert(!uartPaused);
  for(unsigned s=5;s<10;s++){tx=body(s,true);frame(tx,tx,1000000+s*101000);}
  checkComplete(7,0,3,0);assert(linkState.missingEvidence==3);
  assert(Serial.log.find("RECUPERACAO_UART")!=std::string::npos);
  // Timeout final nao conta descarte novamente.
  reset(2);tx=body(0);rx=tx;rx[7]^=1;frame(rx,tx,1000000);
  fakeClock=nextLossAt;serviceTimers();checkComplete(0,1,1,0);
  // Janela nunca comecada nao declara perdas por silencio.
  reset(10);fakeClock=999999999;serviceTimers();assert(classified()==0&&measuring);
  std::cout<<"PASS: RX baselines; 128 single-bit cases over all fields; header corruption; immutable decisions; SUM8 payload and header-only collisions; gaps without double count; duplicates; wrap; malformed/missing/orphan references; parser recovery; UART pause; tail timeout.\n";
}
void targetTests() {
  CaptureSchedule timing;assert(timing.init(80000000));
  for(unsigned i=0;i<8;i++)assert(timing.on[i]<timing.sample[i]&&timing.sample[i]<timing.off[i]);
  assert(CaptureSchedule::elapsed(10,0xFFFFFFFA)==16);
  std::mt19937 rng(12345);uint64_t covered=0;
  for(unsigned mode=0;mode<=4;mode++)for(unsigned t=0;t<10000;t++) {
    uint8_t m[8];chooseTargets(mode,m,[&](){return rng();});unsigned bits=0,bytes=0;
    for(unsigned i=0;i<8;i++){bits+=pop(m[i]);if(m[i])bytes++;if(mode==1)for(unsigned b=0;b<8;b++)if(m[i]&(1U<<b))covered|=uint64_t(1)<<(i*8+b);}
    if(mode==0)assert(bits==0);if(mode==1)assert(bits==1);if(mode==2)assert(bits==2);
    if(mode==3){assert(bytes==1&&bits>=3&&bits<=8);for(auto v:m)if(v){while(!(v&1))v>>=1;assert((v&(v+1))==0);}}
    if(mode==4)assert(bits==2&&m[3]==128&&m[4]==128);
  }
  assert(covered==UINT64_MAX);
  std::cout<<"PASS: 50000 target selections; all 64 bit positions covered; distinct two bits; bursts 3..8 contiguous within byte.\n";
}
// Apenas fixtures SINTETICAS para testar coletor, nunca resultados de bancada.
void fixture(const std::string& prefix,bool crc,unsigned mode,unsigned count,bool skipWindow=false) {
  reset(count,crc);std::mt19937 rng(700+mode+(crc?100:0));
  std::ofstream noise(prefix+"_ruido.log");
  noise<<"# MODO_PROXIMO_QUADRO,"<<mode<<",escopo=CORPO64,versao=2.0\n";
  const char *header="seq,modo,byte_corpo,campo,mascara_alvo_hex,timestamp_us,antes_hex,depois_amostrado_hex,mascara_mudou_hex,mudou,sincronismo_ok,atraso_amostra_max_us,amostras_tardias_hex,pulsos_tardios_hex,duracao_byte_us\n";
  noise<<header;
  for(unsigned s=0;s<count;s++) {
    auto tx=body(s,crc);for(unsigned b=3;b<=6;b++)tx[b]=uint8_t(rng());tx[3]|=128;tx[4]|=128;
    tx[7]=crc?crc8(tx.data(),7):uint8_t(0-sum8(tx.data(),7));
    auto rx=tx;uint8_t masks[8];chooseTargets(mode,masks,[&](){return rng();});
    const int64_t at=1000000+s*101000;
    for(unsigned i=0;i<8;i++) {
      rx[i]&=uint8_t(~masks[i]);const unsigned changed=tx[i]^rx[i];char row[512];
      std::snprintf(row,sizeof(row),"%u,%u,%u,%s,%02X,%d,%02X,%02X,%02X,%s,SIM,0,00,00,91\n",
        s,mode,i,bodyField(i),masks[i],int(at+600000000+10000+3000*i),tx[i],rx[i],changed,changed?"SIM":"NAO");noise<<row;
    }
    if(!skipWindow || s<2 || s>4)frame(rx,tx,at);
  }
  assert(!measuring&&validMeasurement&&classified()==count);
  std::ofstream out(prefix+"_rx.log");out<<Serial.log;
}
int main(int argc,char **argv) {
  functionalTests();targetTests();
  if(argc>1){
    std::string base=argv[1];
    for(bool crc:{false,true})for(unsigned mode=0;mode<=4;mode++)fixture(base+"/"+(crc?"CRC8":"SUM8")+"_"+std::to_string(mode),crc,mode,1000);
    fixture(base+"/SUM8_gap",false,0,10,true);
  }
  std::cout<<"PASS: software only; no electrical measurement or experimental data generated.\n";
}
