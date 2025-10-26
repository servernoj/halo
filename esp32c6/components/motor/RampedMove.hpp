#include "Common.hpp"
#include <cstdint>
#include <vector>

namespace motor {
  // const std::vector<float> profile = { //
  //   1.0, 0.9, 0.6, 0.5
  // };
  const std::vector<float> profile = { //
    1.000f, 0.992f, 0.968f, 0.931f, 0.883f, 0.826f, 0.763f, 0.697f,
    0.633f, 0.571f, 0.514f, 0.466f, 0.429f, 0.402f, 0.382f, 0.250f
  };

  class RampedMove {
    public:
      static constexpr uint32_t stepsPerSegment = 20;
      static std::vector<SegmentData> generateSegments(int32_t degrees, uint32_t factor);

    private:
      static constexpr const char *TAG = "RampedMove";
  };
}