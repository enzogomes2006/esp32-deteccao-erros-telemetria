/* RX COMPLETO v0.2 - candidato separado para ESP32 classico WROOM.
   UART2 115200 8N1, RX GPIO16. Compativel com o TX atual.
   Monitor USB 115200: s=SUM8 c=CRC8 d=10 r=1000 a=10comPausa x=cancelar.
   Prepare RX ANTES de t/g no TX. Padrao SUM8. CPU 240 MHz.

   CAMADA DO ENLACE: usa LEN/seq/CHK recebidos, sem consultar a referencia.
   CAMADA DE AUDITORIA: compara payload com a copia original depois da
   decisao; nao corrige sequencia, aceitacao ou descarte do enlace.

   PREMISSA DO ENSAIO: RUIDO so altera os 32 bits do payload. Portanto,
   seq recebida e usada tambem nos quadros DESCARTADOS. Em um canal que
   possa corromper o cabecalho, seq de um quadro descartado nao e confiavel.
   Divergencia posterior de seq com a referencia INVALIDA a medicao; nao
   altera retroativamente o controle de sequencia. Nao e um RX generico
   para qualquer modelo de erro de cabecalho.

   Quadro: A5 5A C3 3C | 07 seqL seqH p0 p1 p2 p3 CHK.
   Referencia: D5 6A B4 4B | copia dos 8 bytes originais do corpo.
   SUM8 ou CRC-8/SMBUS; cheque sobre os oito bytes, residuo esperado zero.
   Uma tecnica por rodada. Sem ACK/retransmissao. LEN fixo 7.

   Timeouts: 60 ms bloco/copia; 250 ms enlace desde primeiro marcador de
   dados ou ultimo corpo checado. Silencio persistente: passo de 100 ms.
   Uma classificacao por sequencia; descartado nao vira perdido novamente.
   Ausencia de referencia nao vira OK nem prova perda do quadro de dados.

   Numeracao uint16 e comparacao de meia faixa. prepareRun permite origem
   diferente de zero para teste logico de wrap-around. Comandos USB usam
   origem zero e janelas finitas de 10/1000, como o TX atual.

   Em 'a', RUIDO=0: apos checar E AUDITAR seq1, desliga a UART por 350 ms.
   Mede retorno ate a proxima decisao ACEITO; nao e ruido eletrico.
*/
#include <Arduino.h>
#include <esp_timer.h>
#include <cstring>
#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Use ESP32 classico WROOM / ESP32 Dev Module."
#endif

const uint32_t START_MARK=0xA55AC33C, DEBUG_MARK=0xD56AB44B;
const int64_t FRAME_TIMEOUT_US=60000, LINK_TIMEOUT_US=250000;
const int64_t NOMINAL_PERIOD_US=100000, PAUSE_DURATION_US=350000;
const uint16_t MAX_FRAMES=1000;

enum ProtocolState : uint8_t { PROTO_UNSEEN=0, PROTO_ACCEPTED=1, PROTO_REJECTED=2, PROTO_LOST=3 };
enum ResultCode : uint8_t { RESULT_UNCLASSIFIED=0, RESULT_OK=1, RESULT_CORRUPTED=2, RESULT_MISSING=3, RESULT_UNDETECTED=4 };

struct PendingAudit {
  bool active=false, accepted=false;
  uint16_t slot=0, seq=0;
  uint8_t bytes[8]={}, residue=0;
  uint32_t verifyUs=0, intervalUs=0;
  int64_t startedAt=0, decidedAt=0, recoveryUs=-1;
};
PendingAudit pending;
uint8_t protocol[MAX_FRAMES]={}, results[MAX_FRAMES]={};
uint8_t parser=0, pos=0, buffer[8]={};
uint32_t shiftReg=0, totals[4]={};
uint16_t wanted=10, firstSeq=0, nextSlot=0;
bool useCRC=false, measuring=false, streamStarted=false, validMeasurement=true;
bool haveMarker=false, lossEpisode=false;
bool automaticPause=false, pauseDone=false, pauseRequested=false, uartPaused=false;
bool waitingAfterRestore=false;
uint32_t checkedCount=0, acceptedCount=0, rejectedCount=0, ignoredCount=0;
uint32_t noReferenceCount=0, invalidReferenceCount=0, malformedCount=0;
uint32_t verifyMin=0xFFFFFFFFUL, verifyMax=0;
uint64_t verifyTotal=0;
int64_t parserSince=0, markerAt=0, previousMarkerAt=0, nextLossAt=0;
uint32_t markerIntervalUs=0;
int64_t firstLossAt=0, pausedAt=0, restoredAt=0;

uint8_t sum8(const uint8_t *p,size_t n) {
  uint8_t s=0; while(n--) s+=*p++; return s;
}
uint8_t crc8(const uint8_t *p,size_t n) {
  uint8_t c=0;
  while(n--) {
    c^=*p++;
    for(int b=0;b<8;b++) c=(c&0x80)?uint8_t((c<<1)^0x07):uint8_t(c<<1);
  }
  return c;
}
uint16_t getSeq(const uint8_t *p) { return uint16_t(p[1])|(uint16_t(p[2])<<8); }
uint16_t seqForSlot(uint16_t slot) { return uint16_t(firstSeq+slot); }
uint16_t expectedSeq() { return seqForSlot(nextSlot); }
void printHex(const uint8_t *p,size_t n) {
  for(size_t i=0;i<n;i++) Serial.printf("%02X",p[i]);
}
void clearParser() { parser=0; pos=0; shiftReg=0; }
void startUART() {
  Serial2.setRxBufferSize(1024);
  Serial2.begin(115200,SERIAL_8N1,16,17);
  Serial2.setRxFIFOFull(1);
}
void restoreUART(bool report) {
  if(!uartPaused) return;
  startUART(); restoredAt=esp_timer_get_time(); uartPaused=false;
  waitingAfterRestore=report; clearParser();
  if(report) Serial.printf("# RX_REATIVADO,instante_us=%lld,janela_desligada_us=%lld\n",
    (long long)restoredAt,(long long)(restoredAt-pausedAt));
}
void summary() {
  uint32_t count=totals[0]+totals[1]+totals[2]+totals[3];
  bool complete=count==wanted && validMeasurement;
  Serial.printf("# RESUMO,%s,total=%lu,OK=%lu,CORROMPIDO=%lu,PERDIDO=%lu,NAO_DETECTADO=%lu,taxa=",
    useCRC?"CRC8":"SUM8",(unsigned long)count,(unsigned long)totals[0],
    (unsigned long)totals[1],(unsigned long)totals[2],(unsigned long)totals[3]);
  uint32_t den=totals[1]+totals[3];
  if(den) Serial.printf("%.6f\n",double(totals[3])/den); else Serial.println("NA");
  Serial.printf("# VALIDACAO,esperados=%u,sequencias_processadas=%u,nao_classificados=%lu,"
    "sem_referencia=%lu,referencias_invalidas=%lu,quadros_malformados=%lu,"
    "repetidos_atrasados_ou_fora=%lu,rodada_valida=%s\n",wanted,nextSlot,
    (unsigned long)(wanted-count),(unsigned long)noReferenceCount,
    (unsigned long)invalidReferenceCount,(unsigned long)malformedCount,
    (unsigned long)ignoredCount,complete?"SIM":"NAO");
  Serial.printf("# PROTOCOLO,checados=%lu,aceitos=%lu,descartados=%lu,proxima_seq=%u\n",
    (unsigned long)checkedCount,(unsigned long)acceptedCount,(unsigned long)rejectedCount,expectedSeq());
  if(checkedCount) Serial.printf("# TEMPOS_RX,amostras=%lu,verificacao_media_us=%.3f,"
    "verificacao_min_us=%lu,verificacao_max_us=%lu\n",(unsigned long)checkedCount,
    double(verifyTotal)/checkedCount,(unsigned long)verifyMin,(unsigned long)verifyMax);
  if(!complete) Serial.println("# RODADA_INCOMPLETA_OU_INVALIDA: nao usar como tabela definitiva.");
}
void maybeFinish() {
  if(measuring && nextSlot>=wanted && !pending.active) {
    measuring=false; pauseRequested=false; restoreUART(false); summary();
  }
}
void setResult(uint16_t slot,uint8_t result) {
  if(slot>=wanted || results[slot] || !result) return;
  results[slot]=result; totals[result-1]++;
}
void printAuditRow(uint8_t result,const uint8_t *ref,const char *why) {
  const char *names[]={"INDETERMINADO","OK","CORROMPIDO","PERDIDO","NAO_DETECTADO"};
  const char *equal=ref?(memcmp(pending.bytes+3,ref+3,4)==0?"SIM":"NAO"):"NA";
  Serial.printf("%u,%u,%s,%u,%s,%s,%s,%lu,%lu,",pending.seq,pending.seq,
    useCRC?"CRC8":"SUM8",pending.residue,pending.accepted?"ACEITO":"DESCARTADO",
    names[result],equal,(unsigned long)pending.verifyUs,(unsigned long)pending.intervalUs);
  if(pending.recoveryUs>=0) Serial.printf("%.3f,",double(pending.recoveryUs)/1000.0);
  else Serial.printf("-1,");
  printHex(pending.bytes,8); Serial.print(',');
  if(ref) printHex(ref,8); else Serial.printf("NA");
  Serial.printf(",%s\n",why);
}
void finalizeWithoutReference(const char *why) {
  if(!pending.active) return;
  noReferenceCount++; validMeasurement=false;
  uint8_t result=pending.accepted?RESULT_UNCLASSIFIED:RESULT_CORRUPTED;
  setResult(pending.slot,result); printAuditRow(result,nullptr,why);
  pending.active=false;
}
void recordLost(uint16_t slot,const char *why,int64_t now) {
  if(slot>=wanted || protocol[slot]!=PROTO_UNSEEN) return;
  protocol[slot]=PROTO_LOST; setResult(slot,RESULT_MISSING);
  if(!lossEpisode) { lossEpisode=true; firstLossAt=now; }
  Serial.printf("%u,NA,%s,NA,SEM_CHECAGEM,PERDIDO,NA,NA,NA,-1,NA,NA,%s\n",
    seqForSlot(slot),useCRC?"CRC8":"SUM8",why);
}
void reportProtocolRecovery(uint16_t seq,int64_t now) {
  if(lossEpisode) {
    pending.recoveryUs=now-firstLossAt;
    Serial.printf("# RECUPERACAO_PERDA,seq=%u,primeira_deteccao_us=%lld,quadro_aceito_us=%lld,"
      "desde_deteccao_us=%lld,desde_deteccao_ms=%.3f\n",seq,(long long)firstLossAt,
      (long long)now,(long long)pending.recoveryUs,double(pending.recoveryUs)/1000.0);
    lossEpisode=false;
  }
  if(waitingAfterRestore) {
    int64_t dt=now-restoredAt;
    Serial.printf("# RECUPERACAO_UART,seq=%u,reativacao_us=%lld,quadro_aceito_us=%lld,"
      "apos_reativar_us=%lld,apos_reativar_ms=%.3f\n",seq,(long long)restoredAt,
      (long long)now,(long long)dt,double(dt)/1000.0);
    waitingAfterRestore=false;
  }
}
void onDataBody() {
  if(buffer[0]!=7) {
    malformedCount++; Serial.println("# LEN_INVALIDO: sem checagem; perda sera inferida do enlace."); return;
  }
  int64_t t=esp_timer_get_time();
  uint8_t residue=useCRC?crc8(buffer,8):sum8(buffer,8);
  uint32_t verifyUs=uint32_t(esp_timer_get_time()-t);
  int64_t now=esp_timer_get_time();
  uint16_t seq=getSeq(buffer), slot=uint16_t(seq-firstSeq);
  uint16_t delta=uint16_t(seq-expectedSeq());
  if(slot>=wanted || delta>=32768 || protocol[slot]!=PROTO_UNSEEN) {
    if(residue!=0) {
      validMeasurement=false;
      Serial.println("# SEQUENCIA_NAO_CONFIAVEL: descarte fora da ordem; medicao invalida.");
    }
    ignoredCount++; Serial.printf("# REPETIDO_ATRASADO_OU_FORA,seq_rx=%u,esperado=%u\n",seq,expectedSeq()); return;
  }
  // Nao ha leitura de referencia neste caminho. Cabecalho fora do alvo de ruido.
  while(nextSlot<slot) recordLost(nextSlot++,"LACUNA_SEQUENCIA_RECEBIDA",now);
  bool accepted=residue==0;
  protocol[slot]=accepted?PROTO_ACCEPTED:PROTO_REJECTED;
  nextSlot=slot+1;
  nextLossAt=now+LINK_TIMEOUT_US;
  checkedCount++; if(accepted) acceptedCount++; else rejectedCount++;
  verifyTotal+=verifyUs;
  if(verifyUs<verifyMin) verifyMin=verifyUs;
  if(verifyUs>verifyMax) verifyMax=verifyUs;
  pending.active=true; pending.accepted=accepted;
  pending.slot=slot; pending.seq=seq; pending.residue=residue;
  memcpy(pending.bytes,buffer,8);
  pending.verifyUs=verifyUs; pending.intervalUs=markerIntervalUs;
  pending.startedAt=markerAt; pending.decidedAt=now; pending.recoveryUs=-1;
  Serial.printf("# DECISAO_ANTES_DA_COPIA,seq_rx=%u,residuo=%u,decisao=%s,proxima_seq=%u\n",
    seq,residue,accepted?"ACEITO":"DESCARTADO",expectedSeq());
  if(accepted) reportProtocolRecovery(seq,now);
}
void onReference() {
  if(!pending.active) {
    Serial.println("# COPIA_SEM_QUADRO: ignorada; nao move a sequencia nem cria PERDIDO."); return;
  }
  bool refValid=buffer[0]==7 && getSeq(buffer)==pending.seq;
  uint8_t refResidue=useCRC?crc8(buffer,8):sum8(buffer,8);
  if(!refValid || refResidue!=0) {
    invalidReferenceCount++; validMeasurement=false;
    Serial.printf("# REFERENCIA_INVALIDA,seq_rx=%u,seq_copia=%u; controle do enlace preservado.\n",
      pending.seq,getSeq(buffer));
    uint8_t result=pending.accepted?RESULT_UNCLASSIFIED:RESULT_CORRUPTED;
    setResult(pending.slot,result); printAuditRow(result,nullptr,"AUDITORIA_INVALIDA");
    pending.active=false; maybeFinish(); return;
  }
  bool equal=memcmp(pending.bytes+3,buffer+3,4)==0;
  uint8_t result=!pending.accepted?RESULT_CORRUPTED:(equal?RESULT_OK:RESULT_UNDETECTED);
  setResult(pending.slot,result); printAuditRow(result,buffer,"CHECADO_ANTES_DA_COPIA");
  bool triggerPause=automaticPause && !pauseDone && pending.accepted && pending.slot==1;
  pending.active=false;
  if(triggerPause) pauseRequested=true;
  maybeFinish();
}
void beginDataFrame() {
  // Uma referencia ausente invalida a auditoria, mas nao reseta a sequencia.
  finalizeWithoutReference("NOVA_MARCA_SEM_COPIA_ANTERIOR");
  markerAt=esp_timer_get_time();
  markerIntervalUs=haveMarker?uint32_t(markerAt-previousMarkerAt):0;
  previousMarkerAt=markerAt; haveMarker=true;
  if(!streamStarted) { streamStarted=true; nextLossAt=markerAt+LINK_TIMEOUT_US; }
  parser=1; pos=0; parserSince=markerAt; shiftReg=0;
}
void consume(uint8_t b) {
  if(!measuring) return;
  if(parser) {
    buffer[pos++]=b;
    if(pos==8) {
      bool isData=parser==1; clearParser();
      if(isData) onDataBody(); else onReference();
    }
    return;
  }
  shiftReg=(shiftReg<<8)|b;
  if(shiftReg==START_MARK) beginDataFrame();
  else if(shiftReg==DEBUG_MARK) {
    parser=2; pos=0; parserSince=esp_timer_get_time(); shiftReg=0;
  }
}
void pauseUART() {
  pauseRequested=false; pauseDone=true;
  Serial2.end(); pausedAt=esp_timer_get_time(); uartPaused=true;
  clearParser();
  Serial.printf("# RX_DESLIGADO,apos_auditar_seq=%u,instante_us=%lld,duracao_programada_us=%lld\n",
    seqForSlot(1),(long long)pausedAt,(long long)PAUSE_DURATION_US);
}
void serviceTimers() {
  int64_t now=esp_timer_get_time();
  if(uartPaused && now-pausedAt>=PAUSE_DURATION_US) {
    restoreUART(true); now=esp_timer_get_time();
  }
  if(parser && now-parserSince>FRAME_TIMEOUT_US) {
    clearParser(); Serial.println("# TIMEOUT_BLOCO: buscando novo marcador.");
  }
  if(pending.active && now-pending.startedAt>FRAME_TIMEOUT_US) {
    finalizeWithoutReference("TIMEOUT_COPIA");
  }
  if(measuring && streamStarted && nextSlot<wanted && now>=nextLossAt) {
    recordLost(nextSlot++,"TIMEOUT_ENLACE",now);
    nextLossAt+=NOMINAL_PERIOD_US;
  }
  maybeFinish();
}
void prepareRun(uint16_t count,uint16_t origin=0,bool automatic=false) {
  restoreUART(false);
  measuring=true; wanted=count; firstSeq=origin; nextSlot=0;
  streamStarted=false; validMeasurement=true; haveMarker=false; lossEpisode=false;
  automaticPause=automatic; pauseDone=false; pauseRequested=false; waitingAfterRestore=false;
  pending.active=false; clearParser();
  memset(protocol,0,sizeof(protocol)); memset(results,0,sizeof(results)); memset(totals,0,sizeof(totals));
  checkedCount=acceptedCount=rejectedCount=ignoredCount=0;
  noReferenceCount=invalidReferenceCount=malformedCount=0;
  verifyMin=0xFFFFFFFFUL; verifyMax=0; verifyTotal=0;
  markerAt=previousMarkerAt=nextLossAt=firstLossAt=pausedAt=restoredAt=0; markerIntervalUs=0;
  while(Serial2.available()) Serial2.read();
  Serial.printf("# PREPARADO_RX_COMPLETO,%s,%u,primeira_seq=%u,pausa_automatica=%s\n",
    useCRC?"CRC8":"SUM8",wanted,firstSeq,automatic?"SIM":"NAO");
  Serial.println("seq_enlace,seq_rx,tecnica,residuo,decisao,status,payload_confere,verificacao_us,intervalo_inicio_us,recuperacao_ms,corpo_rx_hex,corpo_tx_hex,motivo");
}
void setup() {
  setCpuFrequencyMhz(240); Serial.begin(115200); startUART();
  Serial.println("# RX COMPLETO v0.2 - candidato separado; validar na bancada.");
  Serial.println("# s=SUM8 c=CRC8 d=10 r=1000 a=10comPausa x=cancelar. Padrao SUM8.");
  Serial.println("# Premissa: injetor so altera payload; sequencia e recebida da linha, nunca da copia.");
}
void loop() {
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='x') {
      pauseRequested=false; restoreUART(false);
      finalizeWithoutReference("CANCELAMENTO_SEM_COPIA"); measuring=false; clearParser(); summary();
    } else if(c=='s'||c=='c') {
      if(!measuring) { useCRC=c=='c'; Serial.printf("# TECNICA,%s\n",useCRC?"CRC8":"SUM8"); }
      else Serial.println("# Cancele com x antes de mudar tecnica.");
    } else if(c=='d'||c=='r'||c=='a') prepareRun(c=='r'?1000:10,0,c=='a');
  }
  serviceTimers();
  while(measuring && !uartPaused && !pauseRequested && Serial2.available()) consume(Serial2.read());
  if(measuring && pauseRequested) pauseUART();
  serviceTimers(); delay(1);
}
