#pragma once

#include "hyperverse/math.hpp"
#include "hyperverse/sprite_collision_shape.hpp"

namespace hyperverse {

struct ShapeQueryHit {
  bool hit{false};
  float fraction{0.0F};
};

[[nodiscard]] bool jolt_shapes_overlap(
  SpriteCollisionShape first_shape,
  float first_radius,
  SpriteCollisionShape second_shape,
  float second_radius,
  Vec2 second_position
);
[[nodiscard]] ShapeQueryHit jolt_cast_shape(
  SpriteCollisionShape moving_shape,
  float moving_radius,
  SpriteCollisionShape target_shape,
  float target_radius,
  Vec2 target_position,
  Vec2 motion
);
[[nodiscard]] ShapeQueryHit jolt_raycast_shape(
  SpriteCollisionShape target_shape,
  float target_radius,
  Vec2 target_position,
  Vec2 direction,
  float range
);

}  // namespace hyperverse
