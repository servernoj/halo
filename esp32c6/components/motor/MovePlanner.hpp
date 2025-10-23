#include "Common.hpp"
#include <cstdint>
#include <vector>

namespace motor {
  const std::vector<float> profile_ = { //
    1.000f, 0.994f, 0.977f, 0.951f, 0.917f, 0.875f, 0.827f, 0.773f,
    0.727f, 0.682f, 0.640f, 0.603f, 0.571f, 0.545f, 0.526f, 0.500f
  };

  class MovePlanner {
    public:
      uint32_t base_time_us;
      uint32_t total_time_us;
      uint32_t total_steps;
      int dir;
      double lo;
      double hi;
      std::vector<float> profile;
      // -- constructor
      MovePlanner(uint32_t base_time_us, uint32_t total_time_us, int32_t steps);
      bool isFeasible();
      double estimateSteps(double beta);
      double getBeta();
      std::vector<SegmentData> generateSegments();

    private:
      static constexpr const char *TAG = "MovePlanner";
  };
}