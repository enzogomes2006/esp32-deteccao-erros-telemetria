/* RUIDO CORPO64 v2.0 - CANDIDATO; validar na placa antes de usar em tabelas.
   ESP32 classico WROOM, Arduino-ESP32 3.3.12, CPU 240 MHz.
   Mesmos pinos: limpa GPIO16/UART2, perturbada GPIO18, transistor GPIO19.
   Modos 0..4. Log novo com byte_corpo 0..7 e nome do campo.
   v1.3.1: restaura explicitamente RX da UART2 pela matriz GPIO apos
   configurar CAP0. Corrige conflito com IOMUX nativo de GPIO16 no core3.3.12.
   A inicializacao verifica os registradores e informa a rota compartilhada.

   CAP0 do MCPWM grupo0 registra a borda de descida em hardware.
   CAP1, sem GPIO e sem interrupcao, fornece capturas por software do
   mesmo contador para consultar a passagem do tempo. O ISR de GPIO
   congela CAP0 durante cada byte e usa prazos a partir da borda gravada,
   em vez de subtrair uma latencia estimada de 3 us da entrada no ISR.
   MCPWM e usado apenas como base de tempo, NAO para gerar PWM nos fios.
   O transistor continua gerando pulsos fisicos por GPIO19.

   O timer de captura tem contador de 32 bits. Esperas usam diferencas
   unsigned, validas tambem durante o wrap do contador.
   UART limpa e efeito esperado continuam auditados; avisos nao somem.
   ISR excessivamente atrasado pode perder a primeira borda ou janela;
   este candidato NAO promete corrigir qualquer latencia de interrupcao.
   Conferir tambem os bytes RX com o log do injetor (coletor v2).

   MCPWM0/canais0 e1 sao exclusivos deste sketch. Chamadas LL dependem
   dos headers ESP32 instalados; compilacao verificada em core3.3.12.
   LEN, sequencia, payload e CHK sao alvos. Copia e marcador preservados.
   Usar RX CORPO64 v1 e coletor v3. Mesmos fios e TX anterior.
*/
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <soc/gpio_struct.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <driver/gpio.h>
#include <driver/mcpwm_cap.h>
#include <hal/mcpwm_ll.h>
#include <esp_intr_alloc.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include "capture_schedule.h"
#include "noise_targets.h"
#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Este injetor usa GPIO do ESP32 classico WROOM; nao C3/S3/S2."
#endif
const uint8_t INPUT_PIN=16, SENSE_PIN=18, BASE_PIN=19;
CaptureSchedule schedule;
mcpwm_cap_timer_handle_t captureTimer=nullptr;
mcpwm_cap_channel_handle_t edgeChannel=nullptr, clockChannel=nullptr;
volatile uint32_t entryAgeTicks[8]={};
uint32_t captureHz=0;
const uint32_t INPUT_MASK=1UL<<INPUT_PIN, SENSE_MASK=1UL<<SENSE_PIN, BASE_MASK=1UL<<BASE_PIN;
volatile bool armed=false;
volatile uint8_t byteIndex=0, masks[8]={}, sampledIn[8]={}, sampledOut[8]={}, timingBad[8]={};
volatile int64_t stamps[8]={};
volatile uint8_t lateSamples[8]={}, latePulses[8]={};
volatile uint16_t worstLateUs[8]={}, durationUs[8]={};
portMUX_TYPE byteMux=portMUX_INITIALIZER_UNLOCKED;
uint8_t mode=0, frameMode=0, clean[8]={}, count=0;
uint8_t parser=0; uint32_t shiftReg=0; int64_t since=0;

// LL apenas para operacoes curtas e sem locks dentro do ISR.
static inline uint32_t IRAM_ATTR captureNow() {
  mcpwm_ll_trigger_soft_capture(&MCPWM0,1);
  return mcpwm_ll_capture_get_value(&MCPWM0,1);
}
static inline uint32_t IRAM_ATTR age(uint32_t edge) {
  return uint32_t(captureNow()-edge);
}
static inline uint32_t IRAM_ATTR waitTicks(uint32_t edge,uint32_t target) {
  uint32_t n;
  do { n=age(edge); } while(n<target);
  return n;
}
void IRAM_ATTR onStartBit(void*) {
  if(!armed) return;
  const uint8_t index=byteIndex;
  if(index>=8) { armed=false; GPIO.out_w1tc=BASE_MASK; return; }
  // Congela a borda antes que os bits seguintes possam sobrescrever CAP0.
  mcpwm_ll_capture_enable_channel(&MCPWM0,0,false);
  const uint32_t edge=mcpwm_ll_capture_get_value(&MCPWM0,0);
  const uint32_t arrival=age(edge);
  const int64_t entered=esp_timer_get_time(); // timestamp do log = entrada ISR
  portENTER_CRITICAL_ISR(&byteMux);
  uint8_t mask=masks[index],before=0,after=0,bad=0,sampleLate=0,pulseLate=0;
  uint32_t worst=0;
  entryAgeTicks[index]=arrival;
  stamps[index]=entered;
  // Nao tenta recuperar uma janela cujo primeiro pulso ja passou.
  if(arrival>=schedule.on[0]) { bad=1; sampleLate=0xFF; }
  for(uint8_t b=0;b<8;b++) {
    waitTicks(edge,schedule.on[b]);
    if(mask&(1U<<b)) {
      if(age(edge)>=schedule.sample[b]-schedule.ticksPerUs) {
        bad=1; pulseLate|=1U<<b;
      }
      if(!bad) GPIO.out_w1ts=BASE_MASK;
    } else GPIO.out_w1tc=BASE_MASK;
    waitTicks(edge,schedule.sample[b]);
    const uint32_t levels=GPIO.in;
    const uint32_t observed=age(edge);
    const uint32_t lateness=observed>schedule.sample[b]?observed-schedule.sample[b]:0;
    if(lateness>worst) worst=lateness;
    if(lateness>schedule.ticksPerUs) { bad=1; sampleLate|=1U<<b; }
    if(levels&INPUT_MASK) before|=1U<<b;
    if(levels&SENSE_MASK) after|=1U<<b;
    if(b==7 || !(mask&(1U<<(b+1))) || bad) {
      waitTicks(edge,schedule.off[b]); GPIO.out_w1tc=BASE_MASK;
      if((mask&(1U<<b)) && age(edge)>schedule.off[b]+schedule.ticksPerUs) {
        bad=1; pulseLate|=1U<<b;
      }
    }
  }
  GPIO.out_w1tc=BASE_MASK;
  waitTicks(edge,schedule.finish);
  sampledIn[index]=before; sampledOut[index]=after; timingBad[index]=bad;
  lateSamples[index]=sampleLate; latePulses[index]=pulseLate;
  // Arredonda para cima somente o campo informativo, sem relaxar o limite.
  worstLateUs[index]=uint16_t((worst+schedule.ticksPerUs-1)/schedule.ticksPerUs);
  durationUs[index]=uint16_t(esp_timer_get_time()-entered);
  byteIndex=index+1;
  if(index==7) armed=false;
  GPIO.status_w1tc=INPUT_MASK;
  mcpwm_ll_intr_clear_status(&MCPWM0,MCPWM_LL_EVENT_CAPTURE(0)|MCPWM_LL_EVENT_CAPTURE(1));
  mcpwm_ll_capture_enable_channel(&MCPWM0,0,true);
  portEXIT_CRITICAL_ISR(&byteMux);
}
void setupCaptureClock() {
  mcpwm_capture_timer_config_t tc={};
  tc.group_id=0; tc.clk_src=MCPWM_CAPTURE_CLK_SRC_APB;
  ESP_ERROR_CHECK(mcpwm_new_capture_timer(&tc,&captureTimer));
  ESP_ERROR_CHECK(mcpwm_capture_timer_get_resolution(captureTimer,&captureHz));
  if(!schedule.init(captureHz)) {
    Serial.println("# ERRO_RELOGIO_CAPTURA: configuracao nao suportada.");
    while(true) delay(1000);
  }
  mcpwm_capture_channel_config_t cc={};
  cc.gpio_num=INPUT_PIN; cc.prescale=1; cc.flags.neg_edge=true;
  ESP_ERROR_CHECK(mcpwm_new_capture_channel(captureTimer,&cc,&edgeChannel));
  // O segundo canal nao recebe GPIO e nao registra callback/IRQ.
  cc={}; cc.gpio_num=-1; cc.prescale=1;
  ESP_ERROR_CHECK(mcpwm_new_capture_channel(captureTimer,&cc,&clockChannel));
  ESP_ERROR_CHECK(mcpwm_capture_channel_enable(edgeChannel));
  ESP_ERROR_CHECK(mcpwm_capture_channel_enable(clockChannel));
  ESP_ERROR_CHECK(mcpwm_capture_timer_enable(captureTimer));
  ESP_ERROR_CHECK(mcpwm_capture_timer_start(captureTimer));
  const uint32_t initial=captureNow();
  delay(2);
  const uint32_t clockDelta=captureNow()-initial;
  if(clockDelta<captureHz/1000 || clockDelta>captureHz/10) {
    Serial.println("# ERRO_RELOGIO_CAPTURA: contador sem progressao esperada; injetor desarmado.");
    while(true) delay(1000);
  }
  // Aplicacao dedicada, sem attachInterrupt anterior. Falha e explicita.
  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL3));
  ESP_ERROR_CHECK(gpio_set_intr_type(gpio_num_t(INPUT_PIN),GPIO_INTR_NEGEDGE));
  ESP_ERROR_CHECK(gpio_isr_handler_add(gpio_num_t(INPUT_PIN),onStartBit,nullptr));
  ESP_ERROR_CHECK(gpio_intr_enable(gpio_num_t(INPUT_PIN)));
}
void armFrame() {
  armed=false; GPIO.out_w1tc=BASE_MASK; frameMode=mode;
  for(int i=0;i<8;i++) {
    masks[i]=0; sampledIn[i]=0; sampledOut[i]=0; timingBad[i]=0; stamps[i]=0;
    lateSamples[i]=0; latePulses[i]=0; worstLateUs[i]=0; durationUs[i]=0; entryAgeTicks[i]=0;
  }
  uint8_t selected[8];
  chooseTargets(frameMode,selected,[]() -> uint32_t { return esp_random(); });
  for(uint8_t i=0;i<8;i++) masks[i]=selected[i];
  byteIndex=0; armed=true;
}
// Bits de motivo: 1=amostra limpa difere da UART, 2=tempo fora do limite,
// 4=saida difere do efeito esperado do transistor. Verifica TODOS os bytes.
uint8_t auditByte(uint8_t i) {
  uint8_t reason=0;
  if(sampledIn[i]!=clean[i]) reason|=1;
  if(timingBad[i]) reason|=2;
  uint8_t expected=sampledIn[i] & uint8_t(~masks[i]);
  if(sampledOut[i]!=expected) reason|=4;
  return reason;
}
void logFrame() {
  armed=false; GPIO.out_w1tc=BASE_MASK;
  uint16_t seq=uint16_t(clean[1])|(uint16_t(clean[2])<<8);
  bool sync=(byteIndex==8 && clean[0]==7);
  uint8_t badUART=0, badTiming=0, badEffect=0;
  for(uint8_t i=0;i<8;i++) {
    uint8_t why=auditByte(i);
    if(why&1) badUART|=1U<<i;
    if(why&2) badTiming|=1U<<i;
    if(why&4) badEffect|=1U<<i;
    if(why) sync=false;
  }
  if(!sync) {
    Serial.printf("# SINCRONISMO_INVALIDO,seq=%u,bytes=%u,len=%u,bytes_uart_hex=%02X,bytes_tempo_hex=%02X,bytes_efeito_hex=%02X; nao use esta rodada como evidencia.\n",
      seq,byteIndex,clean[0],unsigned(badUART),unsigned(badTiming),unsigned(badEffect));
    for(uint8_t i=0;i<8;i++) {
      uint8_t why=auditByte(i);
      if(!why) continue;
      Serial.printf("# DIAG,seq=%u,byte_corpo=%u,motivo=%u,uart=%02X,amostra=%02X,saida=%02X,alvo=%02X,atraso_amostra_us=%u,amostras_tardias_hex=%02X,pulsos_tardios_hex=%02X,duracao_us=%u,entrada_isr_ticks=%lu\n",
        seq,i,why,clean[i],unsigned(sampledIn[i]),unsigned(sampledOut[i]),unsigned(masks[i]),
        unsigned(worstLateUs[i]),unsigned(lateSamples[i]),unsigned(latePulses[i]),unsigned(durationUs[i]),(unsigned long)entryAgeTicks[i]);
      // Um detalhe por quadro: imprimir oito linhas poderia bloquear o loop
      // ate o proximo marcador. Os mapas acima indicam TODOS os bytes ruins.
      break;
    }
  }
  for(int i=0;i<8;i++) { // TODOS os bytes: cruza corpo completo com RX/referencia
    uint8_t changed=(sampledIn[i]^sampledOut[i]);
    Serial.printf("%u,%u,%d,%s,%02X,%lld,%02X,%02X,%02X,%s,%s,%u,%02X,%02X,%u\n",seq,frameMode,i,bodyField(i),
      unsigned(masks[i]),(long long)stamps[i],unsigned(sampledIn[i]),unsigned(sampledOut[i]),
      changed,changed?"SIM":"NAO",sync?"SIM":"NAO",unsigned(worstLateUs[i]),
      unsigned(lateSamples[i]),unsigned(latePulses[i]),unsigned(durationUs[i]));
  }
}
void consume(uint8_t b) {
  if(parser==1) {
    clean[count++]=b;
    if(count==8) { logFrame(); parser=0; shiftReg=0; }
    return;
  }
  if(parser==2) { // ignora a copia INTEIRA, mesmo se contiver um marcador
    if(++count==8) { parser=0; shiftReg=0; } return;
  }
  shiftReg=(shiftReg<<8)|b;
  if(shiftReg==0xA55AC33C) {
    armFrame(); parser=1; count=0; since=esp_timer_get_time(); shiftReg=0;
  } else if(shiftReg==0xD56AB44B) { parser=2; count=0; since=esp_timer_get_time(); shiftReg=0; }
}
void setup() {
  setCpuFrequencyMhz(240);
  pinMode(BASE_PIN,OUTPUT); digitalWrite(BASE_PIN,LOW);
  pinMode(SENSE_PIN,INPUT);
  Serial.begin(115200);
  Serial2.setRxBufferSize(1024); Serial2.begin(115200,SERIAL_8N1,INPUT_PIN,17);
  Serial2.setRxFIFOFull(1);
  setupCaptureClock();
  // MCPWM seleciona funcao GPIO no pad16. No core3.3.12, Serial2.begin
  // pode ter escolhido IOMUX nativo em vez de matriz. Refaz a entrada RX
  // pela matriz DEPOIS da captura, mantendo os dois perifericos no GPIO16.
  esp_rom_gpio_connect_in_signal(INPUT_PIN,U2RXD_IN_IDX,false);
  if(!GPIO.func_in_sel_cfg[U2RXD_IN_IDX].sig_in_sel ||
     GPIO.func_in_sel_cfg[U2RXD_IN_IDX].func_sel!=INPUT_PIN ||
     GPIO.func_in_sel_cfg[U2RXD_IN_IDX].sig_in_inv) {
    Serial.println("# ERRO_UART2_RX_COMPARTILHADO: conferir configuracao; injetor desarmado.");
    while(true) delay(1000);
  }
  Serial.println("# UART2_RX_COMPARTILHADO,gpio=16,matriz=SIM");
  Serial.println("# ESCOPO_RUIDO,CORPO64,versao=2.0");
  Serial.println("# RUIDO CORPO64 v2.0 CANDIDATO. 0=off 1=1bit 2=2bits 3=rajada 4=demoSUM8. Padrao0.");
  Serial.printf("# CAPTURA_HW,MCPWM0,CAP0=borda_CAP1=relogio,resolucao_hz=%lu,ajuste_isr_estimado=NAO\n",(unsigned long)captureHz);
  Serial.println("seq,modo,byte_corpo,campo,mascara_alvo_hex,timestamp_us,antes_hex,depois_amostrado_hex,mascara_mudou_hex,mudou,sincronismo_ok,atraso_amostra_max_us,amostras_tardias_hex,pulsos_tardios_hex,duracao_byte_us");
}
void loop() {
  // Expira antes de ler o proximo quadro. Watchdog de software da janela.
  if(parser && esp_timer_get_time()-since>60000) {
    armed=false; GPIO.out_w1tc=BASE_MASK; parser=0; count=0; shiftReg=0;
    Serial.println("# TIMEOUT: injetor desarmado.");
  }
  while(Serial2.available()) consume(Serial2.read());
  while(Serial.available()) {
    char c=Serial.read();
    if(c>='0'&&c<='4') { mode=c-'0'; Serial.printf("# MODO_PROXIMO_QUADRO,%u,escopo=CORPO64,versao=2.0\n",mode); }
  }
  delay(1);
}
