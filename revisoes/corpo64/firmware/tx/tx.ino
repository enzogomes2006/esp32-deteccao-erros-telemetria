/* TX - ESP32 classico WROOM / Arduino-ESP32 3.x.
   UART2: GPIO17 -> resistor 1k -> RX16. GND comum.
   Serial USB 115200: s=SUM8, c=CRC8, g=1000 quadros, t=10, x=parar.
   Selecione a mesma tecnica no RX ANTES de iniciar.

   Protocolo (8N1, 115200): A5 5A C3 3C | 07 seqL seqH p0 p1 p2 p3 chk
   LEN=7 conta os bytes APOS LEN. Check protege LEN, sequencia e payload.
   SUM8: chk=-soma mod 256; residuo incluindo chk = 0.
   CRC-8/SMBUS: poly=07, init=00, refin=false, refout=false, xorout=00;
   chk=CRC dos 7 bytes; residuo incluindo chk = 0. Uma tecnica por rodada.
   Depuracao: D5 6A B4 4B | copia dos 8 bytes originais (fora do protocolo).
   Overhead: 4 marcador + 1 LEN + 2 seq + 1 check = 8 bytes.
   10 ms apos marcador; 3 ms entre bytes do corpo; 10 ms antes da copia.
   Pausas sao idle HIGH, nao mudam os 115200 baud de cada byte.
   Quadros a cada >=100 ms. Sem ACK nem retransmissao.
   Telemetria SINTETICA: 4 bytes aleatorios, p0/p1 com MSB=1 para permitir
   demonstracao fisica reproduzivel da colisao SUM8 no modo extra 4.
*/
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>
#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Use ESP32 classico WROOM (ESP32 Dev Module); nao C3/S3/S2."
#endif

const uint8_t START[] = {0xA5,0x5A,0xC3,0x3C};
const uint8_t DEBUG_MARK[] = {0xD5,0x6A,0xB4,0x4B};
bool useCRC = false, running = false;
uint16_t sequence = 0, remaining = 0;
int64_t nextFrame = 0, previousFrame = 0;

uint8_t crc8(const uint8_t *p, size_t n) {
  uint8_t c = 0;
  while (n--) {
    c ^= *p++;
    for (int b=0;b<8;b++) c = (c & 0x80) ? uint8_t((c<<1)^0x07) : uint8_t(c<<1);
  }
  return c;
}
uint8_t sum8(const uint8_t *p, size_t n) {
  uint8_t s=0; while(n--) s += *p++; return s;
}
void printHex(const uint8_t *p, size_t n) {
  for(size_t i=0;i<n;i++) Serial.printf("%02X",p[i]);
}
void beginRun(uint16_t count) {
  sequence=0; remaining=count; previousFrame=0; running=true;
  nextFrame=esp_timer_get_time()+1000000; // tempo para organizar os monitores
  Serial.printf("# INICIO,%s,%u\n",useCRC?"CRC8":"SUM8",count);
  Serial.println("seq,tecnica,calculo_us,intervalo_us,corpo_tx_hex");
}
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200,SERIAL_8N1,16,17);
  Serial.println("# TX pronto. s=SUM8 c=CRC8 g=1000 t=10 x=parar. Padrao SUM8.");
}
void loop() {
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='x') { running=false; Serial.println("# PARADO"); }
    else if(!running && (c=='s'||c=='c')) {
      useCRC=(c=='c'); Serial.printf("# TECNICA,%s\n",useCRC?"CRC8":"SUM8");
    } else if(!running && (c=='g'||c=='t')) beginRun(c=='g'?1000:10);
  }
  int64_t now=esp_timer_get_time();
  if(!running || now<nextFrame) { delay(1); return; }
  uint8_t body[8]={7,uint8_t(sequence),uint8_t(sequence>>8),0,0,0,0,0};
  uint32_t rnd=esp_random();
  for(int i=0;i<4;i++) body[3+i]=uint8_t(rnd>>(8*i));
  body[3]|=0x80; body[4]|=0x80;
  int64_t t=esp_timer_get_time();
  body[7]=useCRC?crc8(body,7):uint8_t(0-sum8(body,7));
  uint32_t calc=uint32_t(esp_timer_get_time()-t);
  int64_t start=esp_timer_get_time();
  uint32_t dt=previousFrame?uint32_t(start-previousFrame):0;
  previousFrame=start; nextFrame=start+100000;
  Serial2.write(START,4); Serial2.flush();
  delay(10); // permite ao injetor reconhecer o marcador e armar a ISR
  for(int i=0;i<8;i++) {
    Serial2.write(body[i]); Serial2.flush(); delay(3);
  }
  delay(10); // injetor ja desarmado antes da referencia
  Serial2.write(DEBUG_MARK,4); Serial2.write(body,8); Serial2.flush();
  Serial.printf("%u,%s,%lu,%lu,",sequence,useCRC?"CRC8":"SUM8",
                (unsigned long)calc,(unsigned long)dt);
  printHex(body,8); Serial.println();
  sequence++; // uint16_t: volta a zero depois de 65535
  if(--remaining==0) { running=false; Serial.println("# FIM"); }
}
