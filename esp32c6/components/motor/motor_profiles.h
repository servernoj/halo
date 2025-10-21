// Precomputed motor ramp profiles
// Gains are multipliers to the start period (4096us), in range 0.0 to 1.0
// Profiles start at 1.0 and decrease to allow smooth startup

#pragma once

// Linear profile: 16 points from 1.0 to 0.25
const float PROFILE_LINEAR[16] = {
  1.0f, // 4096us
  0.9375f, // ~3839us
  0.875f, // ~3584us
  0.8125f, // ~3328us
  0.75f, // ~3072us
  0.6875f, // ~2816us
  0.625f, // ~2560us
  0.5625f, // ~2304us
  0.5f, // ~2048us
  0.4375f, // ~1792us
  0.375f, // ~1536us
  0.3125f, // ~1280us
  0.25f, // ~1024us
  0.25f, // ~1024us
  0.25f, // ~1024us
  0.25f // ~1024us
};