// Precomputed motor ramp profiles
// Gains are multipliers to the start period (4096us), in range 0.0 to 1.0
// Profiles start at 1.0 and decrease to allow smooth startup

#pragma once
#include <vector>

// Linear profile: 16 points from 1.0 to 0.25
const std::vector<float> PROFILE_LINEAR = { //
  1,   0.95, 0.9, 0.85, //
  0.8, 0.75, 0.7, 0.65, //
  0.6, 0.55, 0.5, 0.45, //
  0.4, 0.35, 0.3, 0.25
};