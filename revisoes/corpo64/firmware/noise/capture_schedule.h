#pragma once
#include <stdint.h>

// Tempos relativos a uma borda capturada. Todos os prazos ficam < 2^31 ticks.
// O contador unsigned admite wrap; nao se compara o instante absoluto.
struct CaptureSchedule {
  uint32_t on[8], sample[8], off[8], finish, ticksPerUs;
  bool init(uint32_t hz, uint32_t baud=115200) {
    if(hz<1000000 || hz%1000000 || baud!=115200) return false;
    ticksPerUs=hz/1000000;
    const uint32_t guard=ticksPerUs; // 1 us de margem em cada borda do bit
    for(unsigned b=0;b<8;b++) {
      on[b]=uint32_t((uint64_t(hz)*(b+1)+baud/2)/baud)+guard;
      sample[b]=uint32_t((uint64_t(hz)*(2*b+3)+baud)/(2*baud));
      off[b]=uint32_t((uint64_t(hz)*(b+2)+baud/2)/baud)-guard;
      if(!(on[b]<sample[b] && sample[b]<off[b])) return false;
    }
    finish=91*ticksPerUs;
    return true;
  }
  static uint32_t elapsed(uint32_t now,uint32_t edge) { return now-edge; }
};
