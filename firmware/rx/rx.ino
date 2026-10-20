/* RX CORPO64 v1.0 - ESP32 classico WROOM, Arduino-ESP32 3.3.12.
   GPIO16 UART2, 115200 8N1. Mesma montagem e TX anterior.
   USB: s=SUM8 c=CRC8 d=10 r=1000 a=10comPausa x=cancelar.
   Preparar antes de iniciar TX. Usar injetor CORPO64 v2 e coletor v3.

   DECISAO: somente LEN e residuo recebidos, ANTES da referencia.
   ENLACE: sequencia de ACEITOS; descartes sao eventos sem seq confiavel.
   AUDITORIA: referencia identifica a posicao experimental apos a decisao.
   Nao corrige a entrega nem o controle do enlace usando a referencia.
   Header corrompido que escape do detector aparece separado no CSV.

   Marcador e referencia preservados no modelo fisico CORPO64. LEN fixo 7:
   parser limitado a oito bytes mesmo se LEN for corrompido. LEN invalido
   e PERDIDO por enquadramento, sem checagem. Sem ACK/retransmissao.
   Referencia ausente/orfa/ambigua invalida a rodada, nunca inventa OK.
   Ruido e pulso fisico 1->0: o alvo pode ja estar em zero.
   Timeouts: 60 ms bloco/auditoria; 250 ms silencio + passo de 100 ms.
   Pausa 'a' somente com ruido 0, apos auditar posicao 1: UART off 350 ms.
   Coletas desta versao: dados/ (19/10/2026). Protocolo na raiz do projeto.
*/
#include <Arduino.h>
#include <esp_timer.h>
#include <cstring>
#include "sequence_link.h"
#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Use ESP32 classico WROOM / ESP32 Dev Module."
#endif

const uint32_t START_MARK=0xA55AC33C, DEBUG_MARK=0xD56AB44B;
const int64_t FRAME_TIMEOUT_US=60000, LINK_TIMEOUT_US=250000;
const int64_t NOMINAL_PERIOD_US=100000, PAUSE_DURATION_US=350000;
const uint16_t MAX_FRAMES=1000;
enum ResultCode : uint8_t { UNCLASSIFIED=0, RESULT_OK=1, RESULT_CORRUPTED=2,
  RESULT_MISSING=3, RESULT_UNDETECTED=4 };
struct PendingAudit {
  bool active=false, decided=false, checked=false, accepted=false, delivered=false;
  uint32_t event=0, verifyUs=0, intervalUs=0;
  uint16_t seq=0, beforeExpected=0, afterExpected=0;
  uint8_t bytes[8]={}, received=0, residue=0;
  int64_t startedAt=0, decidedAt=0, recoveryUs=-1;
  const char *reason="NA";
};
PendingAudit pending;
SequenceLink linkState;
uint8_t results[MAX_FRAMES]={}, parser=0, pos=0, buffer[8]={};
uint32_t shiftReg=0, totals[4]={}, eventCount=0, auditCount=0;
uint32_t checkedCount=0, acceptedCount=0, rejectedCount=0, malformedCount=0;
uint32_t noReferenceCount=0, invalidReferenceCount=0, duplicateAuditCount=0;
uint32_t acceptedHeaderErrors=0, verifyMin=0xFFFFFFFFUL, verifyMax=0;
uint64_t verifyTotal=0;
uint16_t wanted=10, firstSeq=0;
bool useCRC=false, measuring=false, validMeasurement=true, streamStarted=false;
bool haveMarker=false, lastSlotSeen=false, lossEpisode=false;
bool automaticPause=false, pauseDone=false, pauseRequested=false, uartPaused=false;
bool waitingAfterRestore=false;
int64_t parserSince=0, markerAt=0, previousMarkerAt=0, nextLossAt=0;
int64_t firstLossAt=0, pausedAt=0, restoredAt=0;
uint32_t markerIntervalUs=0;

uint8_t sum8(const uint8_t *p,size_t n) {
  uint8_t r=0; while(n--) r+=*p++; return r;
}
uint8_t crc8(const uint8_t *p,size_t n) {
  uint8_t r=0;
  while(n--) { r^=*p++; for(int b=0;b<8;b++) r=(r&128)?uint8_t((r<<1)^7):uint8_t(r<<1); }
  return r;
}
uint8_t residueOf(const uint8_t *p) { return useCRC?crc8(p,8):sum8(p,8); }
uint16_t getSeq(const uint8_t *p) { return uint16_t(p[1])|(uint16_t(p[2])<<8); }
uint16_t seqForSlot(uint16_t slot) { return uint16_t(firstSeq+slot); }
uint32_t classified() { return totals[0]+totals[1]+totals[2]+totals[3]; }
void printHex(const uint8_t *p,size_t n) { for(size_t i=0;i<n;i++) Serial.printf("%02X",p[i]); }
void clearParser() { parser=0; pos=0; shiftReg=0; }
void startUART() {
  Serial2.setRxBufferSize(1024); Serial2.begin(115200,SERIAL_8N1,16,17); Serial2.setRxFIFOFull(1);
}
void restoreUART(bool report) {
  if(!uartPaused) return;
  startUART(); uartPaused=false; restoredAt=esp_timer_get_time();
  waitingAfterRestore=report; clearParser();
  if(report) Serial.printf("# RX_REATIVADO,instante_us=%lld,janela_desligada_us=%lld\n",
    (long long)restoredAt,(long long)(restoredAt-pausedAt));
}
void summary() {
  const bool complete=classified()==wanted && validMeasurement;
  Serial.printf("# RESUMO,%s,total=%lu,OK=%lu,CORROMPIDO=%lu,PERDIDO=%lu,NAO_DETECTADO=%lu,taxa=",
    useCRC?"CRC8":"SUM8",(unsigned long)classified(),(unsigned long)totals[0],
    (unsigned long)totals[1],(unsigned long)totals[2],(unsigned long)totals[3]);
  const uint32_t den=totals[1]+totals[3];
  if(den) Serial.printf("%.6f\n",double(totals[3])/den); else Serial.println("NA");
  Serial.printf("# VALIDACAO,esperados=%u,auditados=%lu,nao_classificados=%lu,sem_referencia=%lu,"
    "referencias_invalidas=%lu,repetidos_auditoria=%lu,cabecalhos_aceitos_divergentes=%lu,rodada_valida=%s\n",
    wanted,(unsigned long)auditCount,(unsigned long)(wanted-classified()),(unsigned long)noReferenceCount,
    (unsigned long)invalidReferenceCount,(unsigned long)duplicateAuditCount,
    (unsigned long)acceptedHeaderErrors,complete?"SIM":"NAO");
  Serial.printf("# ENLACE,checados=%lu,aceitos_detector=%lu,descartados=%lu,malformados=%lu,"
    "proxima_seq=%u,anonimos=%lu,ausencias_evidenciadas=%lu,ambiguidades=%lu,antigos_repetidos=%lu\n",
    (unsigned long)checkedCount,(unsigned long)acceptedCount,(unsigned long)rejectedCount,
    (unsigned long)malformedCount,linkState.expected,(unsigned long)linkState.anonymous,
    (unsigned long)linkState.missingEvidence,(unsigned long)linkState.ambiguities,
    (unsigned long)linkState.oldOrRepeated);
  if(checkedCount) Serial.printf("# TEMPOS_RX,amostras=%lu,verificacao_media_us=%.3f,"
    "verificacao_min_us=%lu,verificacao_max_us=%lu\n",(unsigned long)checkedCount,
    double(verifyTotal)/checkedCount,(unsigned long)verifyMin,(unsigned long)verifyMax);
  if(!complete) Serial.println("# RODADA_INCOMPLETA_OU_INVALIDA: preservar logs; nao usar como tabela definitiva.");
}
void setResult(uint16_t slot,uint8_t result) {
  if(slot>=wanted || results[slot] || !result) return;
  results[slot]=result; totals[result-1]++;
}
void noteMissingEvidence(uint32_t before,int64_t now) {
  if(linkState.missingEvidence<=before) return;
  if(!lossEpisode) { lossEpisode=true; firstLossAt=now; }
  Serial.printf("# AUSENCIA_ENLACE,total=%lu,instante_us=%lld; evidencia agregada,nao duplica eventos auditados.\n",
    (unsigned long)linkState.missingEvidence,(long long)now);
}
void reportRecovery(int64_t now) {
  if(lossEpisode) {
    pending.recoveryUs=now-firstLossAt;
    Serial.printf("# RECUPERACAO_PERDA,seq_rx=%u,primeira_deteccao_us=%lld,aceitacao_us=%lld,desde_deteccao_ms=%.3f\n",
      pending.seq,(long long)firstLossAt,(long long)now,double(pending.recoveryUs)/1000.0);
    lossEpisode=false;
  }
  if(waitingAfterRestore) {
    Serial.printf("# RECUPERACAO_UART,seq_rx=%u,reativacao_us=%lld,aceitacao_us=%lld,apos_reativar_ms=%.3f\n",
      pending.seq,(long long)restoredAt,(long long)now,double(now-restoredAt)/1000.0);
    waitingAfterRestore=false;
  }
}
void finalizeWithoutReference(const char *why) {
  if(!pending.active) return;
  validMeasurement=false; noReferenceCount++;
  Serial.printf("# AUDITORIA_INVALIDA,evento=%lu,motivo=%s; decisao do enlace preservada.\n",
    (unsigned long)pending.event,why);
  pending.active=false;
}
// Perdas sem evento so sao publicadas com evidencia independente da copia.
bool inferAbsentSlots() {
  const uint32_t absent=wanted-classified();
  if(!absent) return true;
  if(!validMeasurement || linkState.ambiguities || acceptedHeaderErrors ||
     linkState.missingEvidence!=absent) return false;
  for(uint16_t slot=0;slot<wanted;slot++) if(!results[slot]) {
    setResult(slot,RESULT_MISSING);
    Serial.printf("0,%u,NA,NA,%u,%u,%s,NA,NA,SEM_CHECAGEM,PERDIDO,NA,NA,NA,NA,NA,-1,NA,NA,"
      "AUSENCIA_COM_EVIDENCIA_ENLACE,NA,NA\n",seqForSlot(slot),linkState.expected,linkState.expected,useCRC?"CRC8":"SUM8");
  }
  return true;
}
void maybeFinish() {
  if(!measuring || pending.active || parser) return;
  const bool complete=classified()==wanted;
  const bool endObserved=lastSlotSeen || eventCount-duplicateAuditCount>=wanted;
  if(!complete && !endObserved && linkState.missingEvidence<wanted-classified()) return;
  if(!complete && !inferAbsentSlots() && !endObserved) return;
  if(classified()!=wanted) validMeasurement=false;
  measuring=false; pauseRequested=false; restoreUART(false); summary();
}
void decideData(bool complete) {
  if(!pending.active || pending.decided) return;
  const int64_t now=esp_timer_get_time();
  pending.beforeExpected=linkState.expected;
  pending.seq=pending.received>=3?getSeq(pending.bytes):0;
  pending.checked=complete && pending.bytes[0]==7;
  pending.reason=complete?(pending.checked?"CHECADO_ANTES_DA_COPIA":"LEN_INVALIDO") : "CORPO_INCOMPLETO";
  if(pending.checked) {
    const int64_t t=esp_timer_get_time(); pending.residue=residueOf(pending.bytes);
    pending.verifyUs=uint32_t(esp_timer_get_time()-t);
    pending.accepted=pending.residue==0; checkedCount++;
    verifyTotal+=pending.verifyUs;
    if(pending.verifyUs<verifyMin) verifyMin=pending.verifyUs;
    if(pending.verifyUs>verifyMax) verifyMax=pending.verifyUs;
    if(pending.accepted) {
      acceptedCount++;
      const uint32_t before=linkState.missingEvidence;
      pending.delivered=linkState.accepted(pending.seq);
      noteMissingEvidence(before,now);
    } else { rejectedCount++; linkState.rejectedOrMalformed(); }
  } else { malformedCount++; linkState.rejectedOrMalformed(); }
  pending.afterExpected=linkState.expected;
  pending.decided=true; pending.decidedAt=esp_timer_get_time();
  nextLossAt=pending.decidedAt+LINK_TIMEOUT_US;
  Serial.printf("# DECISAO_ANTES_DA_COPIA,evento=%lu,seq_rx=",(unsigned long)pending.event);
  if(pending.received>=3) Serial.printf("%u",pending.seq); else Serial.print("NA");
  Serial.print(",residuo=");
  if(pending.checked) Serial.printf("%u",pending.residue); else Serial.print("NA");
  Serial.printf(",decisao=%s,proxima_seq=%u,decisao_us=%lld\n",
    !pending.checked?"SEM_CHECAGEM":(pending.accepted?"ACEITO":"DESCARTADO"),
    pending.afterExpected,(long long)pending.decidedAt);
  if(pending.delivered) reportRecovery(pending.decidedAt);
}
void onDataBody() {
  memcpy(pending.bytes,buffer,8); pending.received=8; decideData(true);
}
void printAuditRow(uint16_t slot,const uint8_t *ref,uint8_t result,int64_t referenceAt) {
  const char *names[]={"INDETERMINADO","OK","CORROMPIDO","PERDIDO","NAO_DETECTADO"};
  const bool full=pending.received==8;
  const bool equal=full && memcmp(pending.bytes+3,ref+3,4)==0;
  const bool headerDiff=full && memcmp(pending.bytes,ref,3)!=0;
  Serial.printf("%lu,%u,",(unsigned long)pending.event,seqForSlot(slot));
  if(pending.received>=3) Serial.printf("%u,",pending.seq); else Serial.print("NA,");
  Serial.printf("%u,%u,%u,%s,",getSeq(ref),pending.beforeExpected,pending.afterExpected,useCRC?"CRC8":"SUM8");
  if(pending.received) Serial.printf("%u,",pending.bytes[0]); else Serial.print("NA,");
  if(pending.checked) Serial.printf("%u,",pending.residue); else Serial.print("NA,");
  Serial.printf("%s,%s,%s,%s,%s,",!pending.checked?"SEM_CHECAGEM":(pending.accepted?"ACEITO":"DESCARTADO"),
    names[result],full?(equal?"SIM":"NAO"):"NA",full?(headerDiff?"SIM":"NAO"):"NA",
    pending.delivered?"SIM":"NAO");
  if(pending.checked) Serial.printf("%lu,",(unsigned long)pending.verifyUs); else Serial.print("NA,");
  Serial.printf("%lu,",(unsigned long)pending.intervalUs);
  if(pending.recoveryUs>=0) Serial.printf("%.3f,",double(pending.recoveryUs)/1000.0); else Serial.print("-1,");
  if(full) printHex(pending.bytes,8); else Serial.print("NA");
  Serial.print(','); printHex(ref,8);
  Serial.printf(",%s,%lld,%lld\n",pending.reason,(long long)pending.decidedAt,(long long)referenceAt);
}
void onReference() {
  if(!pending.active || !pending.decided) {
    validMeasurement=false; invalidReferenceCount++;
    Serial.println("# AUDITORIA_INVALIDA,motivo=COPIA_ORFA; nao cria perda e nao move enlace."); return;
  }
  const int64_t referenceAt=esp_timer_get_time();
  const uint16_t seq=getSeq(buffer),slot=uint16_t(seq-firstSeq);
  if(buffer[0]!=7 || residueOf(buffer)!=0 || slot>=wanted || referenceAt<pending.decidedAt) {
    validMeasurement=false; invalidReferenceCount++;
    Serial.printf("# AUDITORIA_INVALIDA,evento=%lu,motivo=REFERENCIA_INVALIDA,seq_copia=%u\n",
      (unsigned long)pending.event,seq);
    pending.active=false; maybeFinish(); return;
  }
  if(results[slot]) {
    duplicateAuditCount++;
    Serial.printf("# AUDITORIA_REPETIDA,evento=%lu,seq_copia=%u; sem nova classificacao.\n",
      (unsigned long)pending.event,seq);
    pending.active=false; maybeFinish(); return;
  }
  const bool equal=pending.received==8 && memcmp(pending.bytes+3,buffer+3,4)==0;
  const bool headerDiff=pending.received==8 && memcmp(pending.bytes,buffer,3)!=0;
  // Auditoria NUNCA altera linkState.expected ou a decisao guardada.
  if(pending.accepted && headerDiff) acceptedHeaderErrors++;
  const uint8_t result=!pending.checked?RESULT_MISSING:
    (!pending.accepted?RESULT_CORRUPTED:(equal?RESULT_OK:RESULT_UNDETECTED));
  setResult(slot,result); auditCount++; if(slot==wanted-1) lastSlotSeen=true;
  printAuditRow(slot,buffer,result,referenceAt);
  const bool pause=automaticPause && !pauseDone && slot==1 && pending.accepted;
  pending.active=false; if(pause) pauseRequested=true; maybeFinish();
}
void beginDataFrame() {
  finalizeWithoutReference("NOVO_QUADRO_SEM_COPIA_ANTERIOR");
  markerAt=esp_timer_get_time();
  markerIntervalUs=haveMarker?uint32_t(markerAt-previousMarkerAt):0;
  previousMarkerAt=markerAt; haveMarker=true; streamStarted=true;
  nextLossAt=markerAt+LINK_TIMEOUT_US;
  pending=PendingAudit(); pending.active=true; pending.event=++eventCount;
  pending.startedAt=markerAt; pending.intervalUs=markerIntervalUs;
  parser=1; pos=0; parserSince=markerAt; shiftReg=0;
}
void consume(uint8_t b) {
  if(!measuring) return;
  if(parser) {
    buffer[pos++]=b;
    if(pos==8) {
      const bool isData=parser==1; clearParser();
      if(isData) onDataBody(); else onReference();
    }
    return;
  }
  shiftReg=(shiftReg<<8)|b;
  if(shiftReg==START_MARK) beginDataFrame();
  else if(shiftReg==DEBUG_MARK) { parser=2; pos=0; parserSince=esp_timer_get_time(); shiftReg=0; }
}
void pauseUART() {
  pauseRequested=false; pauseDone=true;
  Serial2.end(); pausedAt=esp_timer_get_time(); uartPaused=true; clearParser();
  Serial.printf("# RX_DESLIGADO,apos_auditar_seq=%u,instante_us=%lld,duracao_programada_us=%lld\n",
    seqForSlot(1),(long long)pausedAt,(long long)PAUSE_DURATION_US);
}
void serviceTimers() {
  int64_t now=esp_timer_get_time();
  if(uartPaused && now-pausedAt>=PAUSE_DURATION_US) { restoreUART(true); now=esp_timer_get_time(); }
  if(parser && now-parserSince>FRAME_TIMEOUT_US) {
    if(parser==1 && pending.active && !pending.decided) {
      pending.received=pos; memcpy(pending.bytes,buffer,pos); decideData(false);
    }
    clearParser(); Serial.println("# TIMEOUT_BLOCO: buscando novo marcador.");
  }
  if(pending.active && now-pending.startedAt>FRAME_TIMEOUT_US) finalizeWithoutReference("TIMEOUT_COPIA");
  if(measuring && streamStarted && now>=nextLossAt) {
    const uint32_t before=linkState.missingEvidence;
    linkState.silence(); noteMissingEvidence(before,now); nextLossAt+=NOMINAL_PERIOD_US;
  }
  maybeFinish();
}
void prepareRun(uint16_t count,uint16_t origin=0,bool pause=false) {
  restoreUART(false); wanted=count<=MAX_FRAMES?count:MAX_FRAMES; firstSeq=origin;
  linkState.reset(origin); pending=PendingAudit(); clearParser(); memset(results,0,sizeof(results));
  memset(totals,0,sizeof(totals)); eventCount=auditCount=0;
  checkedCount=acceptedCount=rejectedCount=malformedCount=0;
  noReferenceCount=invalidReferenceCount=duplicateAuditCount=acceptedHeaderErrors=0;
  verifyMin=0xFFFFFFFFUL; verifyMax=0; verifyTotal=0;
  measuring=true; validMeasurement=true; streamStarted=haveMarker=lastSlotSeen=lossEpisode=false;
  automaticPause=pause; pauseDone=pauseRequested=waitingAfterRestore=false;
  markerAt=previousMarkerAt=nextLossAt=firstLossAt=pausedAt=restoredAt=0; markerIntervalUs=0;
  while(Serial2.available()) Serial2.read();
  Serial.printf("# PREPARADO_RX_CORPO64,%s,%u,primeira_seq=%u,pausa_automatica=%s,versao=1.0\n",
    useCRC?"CRC8":"SUM8",wanted,origin,pause?"SIM":"NAO");
  Serial.println("evento_id,seq_experimento,seq_rx,seq_referencia,seq_enlace_esperada,seq_enlace_proxima,tecnica,len_rx,residuo,decisao,status,payload_confere,cabecalho_diverge,entregue_aplicacao,verificacao_us,intervalo_inicio_us,recuperacao_ms,corpo_rx_hex,corpo_tx_hex,motivo,decisao_us,referencia_us");
}
void setup() {
  setCpuFrequencyMhz(240); Serial.begin(115200); startUART();
  Serial.println("# RX CORPO64 v1.0: candidato para validacao fisica.");
  Serial.println("# s=SUM8 c=CRC8 d=10 r=1000 a=10comPausa x=cancelar. Padrao SUM8.");
}
void loop() {
  while(Serial.available()) {
    const char c=Serial.read();
    if(c=='x') {
      if(measuring) { finalizeWithoutReference("CANCELAMENTO"); measuring=false; summary(); }
      pauseRequested=false; restoreUART(false); clearParser();
    } else if(c=='s'||c=='c') {
      if(!measuring) { useCRC=c=='c'; Serial.printf("# TECNICA,%s\n",useCRC?"CRC8":"SUM8"); }
      else Serial.println("# Cancele com x antes de mudar tecnica.");
    } else if(c=='d'||c=='r'||c=='a') {
      if(measuring) Serial.println("# Cancele com x antes de preparar outra rodada.");
      else prepareRun(c=='r'?1000:10,0,c=='a');
    }
  }
  serviceTimers();
  while(measuring && !uartPaused && !pauseRequested && Serial2.available()) consume(Serial2.read());
  if(measuring && pauseRequested) pauseUART();
  serviceTimers(); delay(1);
}
