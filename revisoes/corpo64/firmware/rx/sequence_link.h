#pragma once
#include <stdint.h>

// Nao recebe referencia de depuracao. Descartes nao tem sequencia confiavel.
struct SequenceLink {
  uint16_t expected=0;
  uint32_t anonymous=0, silenceSinceAnchor=0, missingEvidence=0;
  uint32_t ambiguities=0, oldOrRepeated=0;
  void reset(uint16_t origin) { *this=SequenceLink(); expected=origin; }
  void rejectedOrMalformed() { anonymous++; }
  void silence() { silenceSinceAnchor++; missingEvidence++; }
  bool accepted(uint16_t seq) {
    const uint16_t delta=uint16_t(seq-expected);
    if(delta>=32768) { oldOrRepeated++; return false; }
    if(delta<anonymous || uint32_t(delta)-anonymous<silenceSinceAnchor) {
      ambiguities++;
    } else {
      missingEvidence+=uint32_t(delta)-anonymous-silenceSinceAnchor;
    }
    expected=uint16_t(seq+1);
    anonymous=0; silenceSinceAnchor=0;
    return true;
  }
};
