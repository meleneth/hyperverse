#pragma once

#include "hyperverse/math.hpp"

namespace hyperverse {

struct SphereCastHit {
  bool hit{false};
  float fraction{0.0F};
};

[[nodiscard]] bool circles_overlap(Vec2 center_delta, float combined_radius);
[[nodiscard]] SphereCastHit cast_circle(Vec2 target_delta, Vec2 motion, float combined_radius);
[[nodiscard]] SphereCastHit raycast_circle(Vec2 target_delta, Vec2 direction, float range, float radius);

}  // namespace hyperverse
