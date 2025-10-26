#include <cstdint>

namespace motor {
  class StepMode {
    public:
      enum class ModeBits : uint8_t {
        FixedFull = 0b1000,
        FixedHalf = 0b1001,
        FixedQuarter = 0b1010,
        FixedEighth = 0b1011,
        FixedSixteenth = 0b1100,
        FixedThirtySecond = 0b1101,
        FixedSixtyFourth = 0b1110,
        FixedOneTwentyEighth = 0b1111,
      };
      StepMode(ModeBits modeBits);
      StepMode();
      uint16_t getFactor();
      ModeBits getModeBits();
      void setFactor(uint16_t factor);
      void setModeBits(ModeBits modeBits);

    private:
      ModeBits modeBits_;
      ModeBits factorToModeBits_(uint16_t factor);
  };
}