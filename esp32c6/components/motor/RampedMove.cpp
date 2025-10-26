#include <algorithm>
#include <cmath>
#include <vector>

#include "Common.hpp"
#include "RampedMove.hpp"

namespace motor {
  std::vector<SegmentData> RampedMove::generateSegments(int32_t degrees, uint32_t factor) {
    // Compute total number of steps
    int32_t dir = degrees > 0 ? +1 : -1;
    uint32_t totalSteps = std::round(std::abs(degrees) * STEPS_PER_REVOLUTION * factor / 360.0f);

    // Compute middle section size and profile cut point
    int32_t midSegmentSize = static_cast<int32_t>(totalSteps)
                             - static_cast<int32_t>(profile.size() * stepsPerSegment * factor * 2);
    size_t profileCutPoint
      = midSegmentSize >= 0 //
          ? profile.size() //
          : static_cast<size_t>(std::round(totalSteps / 2.0f / stepsPerSegment / factor)); //

    // Build the ramp-up (start) sequence
    std::vector<SegmentData> start;
    start.reserve(profileCutPoint);
    for (size_t i = 0; i < profileCutPoint; ++i) {
      SegmentData seg;
      seg.steps = stepsPerSegment * factor * dir;
      seg.period_us = static_cast<uint32_t>(std::round(BASE_PERIOD_US * profile[i]));
      start.push_back(seg);
    }

    // Ramp-down (end) is reversed copy of start
    std::vector<SegmentData> end = start;
    std::reverse(end.begin(), end.end());

    // Optional middle segment
    std::vector<SegmentData> middle;
    if (midSegmentSize > 0) {
      SegmentData midSeg;
      midSeg.steps = midSegmentSize * dir;
      midSeg.period_us = end.front().period_us;
      middle.push_back(midSeg);
    }

    // Merge all parts
    std::vector<SegmentData> plan;
    plan.reserve(start.size() + middle.size() + end.size());
    plan.insert(plan.end(), start.begin(), start.end());
    plan.insert(plan.end(), middle.begin(), middle.end());
    plan.insert(plan.end(), end.begin(), end.end());

    return plan;
  }
}