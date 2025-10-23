#include <algorithm>
#include <cmath>
#include <vector>

#include "MovePlanner.hpp"

#define BETA_LO -10.0
#define BETA_HI +10.0
#define MAX_ITER 100
#define TOLERANCE 1e-5

namespace motor {
  MovePlanner::MovePlanner(uint32_t base_time_us, uint32_t total_time_us, uint32_t total_steps)
      : base_time_us(base_time_us), total_time_us(total_time_us), total_steps(total_steps), lo(BETA_LO),
        hi(BETA_HI) {
    float scale = profile_.front();
    std::transform(
      profile_.begin(), //
      profile_.end(), //
      std::back_inserter(profile), //
      [scale](float x) { return x / scale; }
    );
  }
  bool MovePlanner::isFeasible() {
    bool isTotalTimeLargeEnough = total_time_us >= total_steps * profile.back() * base_time_us;
    bool isTotalTimeSmallEnough = total_time_us <= total_steps * profile.front() * base_time_us;
    int fLo = (int)estimateSteps(lo) - total_steps;
    int fHi = (int)estimateSteps(hi) - total_steps;
    bool canConverge = fLo * fHi < 0;
    return isTotalTimeLargeEnough && isTotalTimeSmallEnough && canConverge;
  }
  double MovePlanner::estimateSteps(double beta) {
    const size_t N = profile.size();
    std::vector<double> w(N);
    double W = 0.0f;
    for (size_t i = 0; i < N; ++i) {
      w[i] = std::pow((double)profile[i], -beta);
      W += w[i];
    }
    double steps = 0.0f;
    for (size_t i = 0; i < N; ++i) {
      double segment_time = (total_time_us * w[i]) / W;
      steps += segment_time / (profile[i] * base_time_us);
    }
    return steps;
  }
  double MovePlanner::getBeta() {
    double beta = 0;
    double fMid;
    for (int iter = 0; iter < MAX_ITER; iter++) {
      beta = 0.5 * (lo + hi);
      fMid = estimateSteps(beta) - (double)total_steps;
      if (std::abs(fMid) <= TOLERANCE) {
        break;
      }
      if (fMid > 0) {
        hi = beta;
      } else {
        lo = beta;
      }
    }
    return beta;
  }
  std::vector<SegmentData> MovePlanner::generateSegments() {
    size_t N = profile.size();
    std::vector<SegmentData> segments(N);
    double beta = getBeta();
    std::vector<double> w(N);
    double W = 0.0f;
    for (size_t i = 0; i < N; i++) {
      w[i] = std::pow((double)profile[i], -beta);
      W += w[i];
    }
    for (size_t i = 0; i < N; i++) {
      double segment_time_us = total_time_us * w.at(i) / W;
      uint32_t period_us = std::round(base_time_us * profile.at(i));
      uint32_t steps = std::round(segment_time_us / period_us);
      segments.at(i) = SegmentData {.steps = steps, .period_us = period_us};
    }
    return segments;
  }
}