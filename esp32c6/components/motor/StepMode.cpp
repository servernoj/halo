#include "StepMode.hpp"

namespace motor {
  StepMode::StepMode() : modeBits_(StepMode::ModeBits::FixedFull) {}
  StepMode::StepMode(ModeBits modeBits) : modeBits_(modeBits) {}
  uint16_t StepMode::getFactor() {
    switch (modeBits_) {
      case ModeBits::FixedFull:
        return 1;
      case ModeBits::FixedHalf:
        return 2;
      case ModeBits::FixedQuarter:
        return 4;
      case ModeBits::FixedEighth:
        return 8;
      case ModeBits::FixedSixteenth:
        return 16;
      case ModeBits::FixedThirtySecond:
        return 32;
      case ModeBits::FixedSixtyFourth:
        return 64;
      case ModeBits::FixedOneTwentyEighth:
        return 128;
      default:
        return 1;
    }
  }
  StepMode::ModeBits StepMode::factorToModeBits_(uint16_t factor) {
    switch (factor) {
      case 1:
        return ModeBits::FixedFull;
      case 2:
        return ModeBits::FixedHalf;
      case 4:
        return ModeBits::FixedQuarter;
      case 8:
        return ModeBits::FixedEighth;
      case 16:
        return ModeBits::FixedSixteenth;
      case 32:
        return ModeBits::FixedThirtySecond;
      case 64:
        return ModeBits::FixedSixtyFourth;
      case 128:
        return ModeBits::FixedOneTwentyEighth;
    }
    return ModeBits::FixedFull;
  }
  void StepMode::setFactor(uint16_t factor) { modeBits_ = factorToModeBits_(factor); }
  void StepMode::setModeBits(ModeBits modeBits) { modeBits_ = modeBits; }
  StepMode::ModeBits StepMode::getModeBits() { return modeBits_; }
}