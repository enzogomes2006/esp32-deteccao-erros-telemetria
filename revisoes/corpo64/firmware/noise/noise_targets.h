#pragma once
#include <stdint.h>

// Posicoes no corpo completo. O gerador aleatorio fica fora do ISR.
inline void setTargetBit(uint8_t masks[8], uint8_t position) {
  masks[position/8] |= uint8_t(1U << (position%8));
}
template<class Random> void chooseTargets(uint8_t mode, uint8_t masks[8], Random random) {
  for(unsigned i=0;i<8;i++) masks[i]=0;
  if(mode==1 || mode==2) {
    const uint8_t a=random()%64;
    setTargetBit(masks,a);
    if(mode==2) {
      uint8_t b; do { b=random()%64; } while(a==b);
      setTargetBit(masks,b);
    }
  } else if(mode==3) {
    const uint8_t index=random()%8, length=3+random()%6;
    const uint8_t first=random()%(9-length);
    masks[index]=uint8_t(((1U<<length)-1)<<first);
  } else if(mode==4) {
    masks[3]=0x80; masks[4]=0x80;
  }
}
inline const char *bodyField(uint8_t i) {
  const char *names[]={"LEN","SEQ_L","SEQ_H","P0","P1","P2","P3","CHK"};
  return i<8?names[i]:"INVALIDO";
}
