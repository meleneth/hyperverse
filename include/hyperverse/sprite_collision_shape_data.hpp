#pragma once

#include "hyperverse/sprite_collision_shape.hpp"

#include <cstddef>

namespace hyperverse {

struct SpriteCollisionPartView {
  const Vec2* points{nullptr};
  std::size_t point_count{0};
};

struct SpriteCollisionShapeView {
  const SpriteCollisionPartView* parts{nullptr};
  std::size_t part_count{0};
};

[[nodiscard]] SpriteCollisionShapeView sprite_collision_shape_data(SpriteCollisionShape shape);

}  // namespace hyperverse
