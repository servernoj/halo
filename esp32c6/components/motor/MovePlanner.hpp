#include "Common.hpp"
#include <cstdint>
#include <vector>

namespace motor {
  const std::vector<float> profile_ = { //
    1,   0.95, 0.9, 0.85, //
    0.8, 0.75, 0.7, 0.65, //
    0.6, 0.55, 0.5, 0.45, //
    0.4, 0.35, 0.3, 0.25
  };

  class MovePlanner {
    public:
      uint32_t base_time_us;
      uint32_t total_time_us;
      uint32_t total_steps;
      double lo;
      double hi;
      std::vector<float> profile;
      // -- constructor
      MovePlanner(uint32_t base_time_us, uint32_t total_time_us, uint32_t total_steps);
      bool isFeasible();
      double estimateSteps(double beta);
      double getBeta();
      std::vector<SegmentData> generateSegments();

    private:
      static constexpr const char *TAG = "MovePlanner";
  };
}