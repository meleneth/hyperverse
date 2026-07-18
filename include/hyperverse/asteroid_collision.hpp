#pragma once

#include <algorithm>

namespace hyperverse {

constexpr float AsteroidSolidRadiusScale = 0.62F;

[[nodiscard]] inline float asteroid_solid_radius(float asteroid_radius) {
  return std::max(1.0F, asteroid_radius * AsteroidSolidRadiusScale);
}

}  // namespace hyperverse
