#include "hyperverse/sprite_collision_shape_data.hpp"

#include <array>

namespace hyperverse {
namespace {

constexpr std::array<Vec2, 6> ship_part_0_points{{
  Vec2{.x = -0.171875F, .y = 0.765625F},
  Vec2{.x = 0.171875F, .y = 0.765625F},
  Vec2{.x = 0.046875F, .y = 0.953125F},
  Vec2{.x = 0.015625F, .y = 0.984375F},
  Vec2{.x = -0.015625F, .y = 0.984375F},
  Vec2{.x = -0.046875F, .y = 0.953125F},
}};

constexpr std::array<Vec2, 5> ship_part_1_points{{
  Vec2{.x = -0.359375F, .y = 0.515625F},
  Vec2{.x = 0.328125F, .y = 0.515625F},
  Vec2{.x = 0.296875F, .y = 0.578125F},
  Vec2{.x = 0.203125F, .y = 0.734375F},
  Vec2{.x = -0.203125F, .y = 0.734375F},
}};

constexpr std::array<Vec2, 6> ship_part_2_points{{
  Vec2{.x = -0.515625F, .y = 0.265625F},
  Vec2{.x = 0.515625F, .y = 0.265625F},
  Vec2{.x = 0.515625F, .y = 0.296875F},
  Vec2{.x = 0.359375F, .y = 0.484375F},
  Vec2{.x = -0.390625F, .y = 0.484375F},
  Vec2{.x = -0.515625F, .y = 0.296875F},
}};

constexpr std::array<Vec2, 6> ship_part_3_points{{
  Vec2{.x = -0.671875F, .y = 0.015625F},
  Vec2{.x = 0.671875F, .y = 0.015625F},
  Vec2{.x = 0.671875F, .y = 0.046875F},
  Vec2{.x = 0.515625F, .y = 0.234375F},
  Vec2{.x = -0.515625F, .y = 0.234375F},
  Vec2{.x = -0.671875F, .y = 0.046875F},
}};

constexpr std::array<Vec2, 6> ship_part_4_points{{
  Vec2{.x = -0.828125F, .y = -0.234375F},
  Vec2{.x = 0.828125F, .y = -0.234375F},
  Vec2{.x = 0.828125F, .y = -0.171875F},
  Vec2{.x = 0.671875F, .y = -0.015625F},
  Vec2{.x = -0.671875F, .y = -0.015625F},
  Vec2{.x = -0.828125F, .y = -0.171875F},
}};

constexpr std::array<Vec2, 8> ship_part_5_points{{
  Vec2{.x = -0.984375F, .y = -0.484375F},
  Vec2{.x = 0.984375F, .y = -0.484375F},
  Vec2{.x = 0.984375F, .y = -0.390625F},
  Vec2{.x = 0.953125F, .y = -0.359375F},
  Vec2{.x = 0.828125F, .y = -0.265625F},
  Vec2{.x = -0.828125F, .y = -0.265625F},
  Vec2{.x = -0.953125F, .y = -0.359375F},
  Vec2{.x = -0.984375F, .y = -0.390625F},
}};

constexpr std::array<Vec2, 4> ship_part_6_points{{
  Vec2{.x = -0.984375F, .y = -0.734375F},
  Vec2{.x = 0.984375F, .y = -0.734375F},
  Vec2{.x = 0.984375F, .y = -0.515625F},
  Vec2{.x = -0.984375F, .y = -0.515625F},
}};

constexpr std::array<Vec2, 8> ship_part_7_points{{
  Vec2{.x = -0.953125F, .y = -0.765625F},
  Vec2{.x = -0.890625F, .y = -0.890625F},
  Vec2{.x = -0.859375F, .y = -0.921875F},
  Vec2{.x = -0.171875F, .y = -0.984375F},
  Vec2{.x = 0.171875F, .y = -0.984375F},
  Vec2{.x = 0.859375F, .y = -0.921875F},
  Vec2{.x = 0.890625F, .y = -0.890625F},
  Vec2{.x = 0.953125F, .y = -0.765625F},
}};

constexpr std::array<SpriteCollisionPartView, 8> ship_parts{{
  SpriteCollisionPartView{.points = ship_part_0_points.data(), .point_count = ship_part_0_points.size()},
  SpriteCollisionPartView{.points = ship_part_1_points.data(), .point_count = ship_part_1_points.size()},
  SpriteCollisionPartView{.points = ship_part_2_points.data(), .point_count = ship_part_2_points.size()},
  SpriteCollisionPartView{.points = ship_part_3_points.data(), .point_count = ship_part_3_points.size()},
  SpriteCollisionPartView{.points = ship_part_4_points.data(), .point_count = ship_part_4_points.size()},
  SpriteCollisionPartView{.points = ship_part_5_points.data(), .point_count = ship_part_5_points.size()},
  SpriteCollisionPartView{.points = ship_part_6_points.data(), .point_count = ship_part_6_points.size()},
  SpriteCollisionPartView{.points = ship_part_7_points.data(), .point_count = ship_part_7_points.size()},
}};

constexpr std::array<Vec2, 7> rock_part_0_points{{
  Vec2{.x = -0.359375F, .y = 0.515625F},
  Vec2{.x = 0.296875F, .y = 0.515625F},
  Vec2{.x = 0.265625F, .y = 0.578125F},
  Vec2{.x = 0.140625F, .y = 0.640625F},
  Vec2{.x = -0.078125F, .y = 0.640625F},
  Vec2{.x = -0.265625F, .y = 0.578125F},
  Vec2{.x = -0.328125F, .y = 0.546875F},
}};

constexpr std::array<Vec2, 6> rock_part_1_points{{
  Vec2{.x = -0.515625F, .y = 0.265625F},
  Vec2{.x = 0.296875F, .y = 0.265625F},
  Vec2{.x = 0.296875F, .y = 0.484375F},
  Vec2{.x = -0.390625F, .y = 0.484375F},
  Vec2{.x = -0.484375F, .y = 0.390625F},
  Vec2{.x = -0.515625F, .y = 0.296875F},
}};

constexpr std::array<Vec2, 5> rock_part_2_points{{
  Vec2{.x = -0.546875F, .y = 0.015625F},
  Vec2{.x = 0.390625F, .y = 0.015625F},
  Vec2{.x = 0.390625F, .y = 0.109375F},
  Vec2{.x = 0.296875F, .y = 0.234375F},
  Vec2{.x = -0.546875F, .y = 0.234375F},
}};

constexpr std::array<Vec2, 6> rock_part_3_points{{
  Vec2{.x = -0.515625F, .y = -0.046875F},
  Vec2{.x = -0.171875F, .y = -0.234375F},
  Vec2{.x = 0.453125F, .y = -0.234375F},
  Vec2{.x = 0.453125F, .y = -0.171875F},
  Vec2{.x = 0.390625F, .y = -0.015625F},
  Vec2{.x = -0.515625F, .y = -0.015625F},
}};

constexpr std::array<Vec2, 6> rock_part_4_points{{
  Vec2{.x = -0.140625F, .y = -0.359375F},
  Vec2{.x = -0.078125F, .y = -0.484375F},
  Vec2{.x = 0.359375F, .y = -0.484375F},
  Vec2{.x = 0.453125F, .y = -0.390625F},
  Vec2{.x = 0.484375F, .y = -0.265625F},
  Vec2{.x = -0.140625F, .y = -0.265625F},
}};

constexpr std::array<Vec2, 5> rock_part_5_points{{
  Vec2{.x = -0.046875F, .y = -0.515625F},
  Vec2{.x = 0.015625F, .y = -0.578125F},
  Vec2{.x = 0.203125F, .y = -0.578125F},
  Vec2{.x = 0.296875F, .y = -0.546875F},
  Vec2{.x = 0.359375F, .y = -0.515625F},
}};

constexpr std::array<SpriteCollisionPartView, 6> rock_parts{{
  SpriteCollisionPartView{.points = rock_part_0_points.data(), .point_count = rock_part_0_points.size()},
  SpriteCollisionPartView{.points = rock_part_1_points.data(), .point_count = rock_part_1_points.size()},
  SpriteCollisionPartView{.points = rock_part_2_points.data(), .point_count = rock_part_2_points.size()},
  SpriteCollisionPartView{.points = rock_part_3_points.data(), .point_count = rock_part_3_points.size()},
  SpriteCollisionPartView{.points = rock_part_4_points.data(), .point_count = rock_part_4_points.size()},
  SpriteCollisionPartView{.points = rock_part_5_points.data(), .point_count = rock_part_5_points.size()},
}};

constexpr std::array<Vec2, 4> particle_part_0_points{{
  Vec2{.x = -0.687500F, .y = 0.562500F},
  Vec2{.x = 0.562500F, .y = 0.562500F},
  Vec2{.x = 0.437500F, .y = 0.687500F},
  Vec2{.x = -0.562500F, .y = 0.687500F},
}};

constexpr std::array<Vec2, 4> particle_part_1_points{{
  Vec2{.x = -0.812500F, .y = 0.312500F},
  Vec2{.x = 0.812500F, .y = 0.312500F},
  Vec2{.x = 0.687500F, .y = 0.437500F},
  Vec2{.x = -0.687500F, .y = 0.437500F},
}};

constexpr std::array<Vec2, 4> particle_part_2_points{{
  Vec2{.x = -0.812500F, .y = 0.062500F},
  Vec2{.x = 0.812500F, .y = 0.062500F},
  Vec2{.x = 0.812500F, .y = 0.187500F},
  Vec2{.x = -0.812500F, .y = 0.187500F},
}};

constexpr std::array<Vec2, 4> particle_part_3_points{{
  Vec2{.x = -0.812500F, .y = -0.187500F},
  Vec2{.x = 0.812500F, .y = -0.187500F},
  Vec2{.x = 0.812500F, .y = -0.062500F},
  Vec2{.x = -0.812500F, .y = -0.062500F},
}};

constexpr std::array<Vec2, 4> particle_part_4_points{{
  Vec2{.x = -0.812500F, .y = -0.437500F},
  Vec2{.x = 0.687500F, .y = -0.437500F},
  Vec2{.x = 0.812500F, .y = -0.312500F},
  Vec2{.x = -0.812500F, .y = -0.312500F},
}};

constexpr std::array<Vec2, 4> particle_part_5_points{{
  Vec2{.x = -0.812500F, .y = -0.562500F},
  Vec2{.x = -0.687500F, .y = -0.687500F},
  Vec2{.x = 0.562500F, .y = -0.687500F},
  Vec2{.x = 0.687500F, .y = -0.562500F},
}};

constexpr std::array<Vec2, 4> particle_part_6_points{{
  Vec2{.x = -0.437500F, .y = -0.812500F},
  Vec2{.x = -0.187500F, .y = -0.937500F},
  Vec2{.x = 0.312500F, .y = -0.937500F},
  Vec2{.x = 0.562500F, .y = -0.812500F},
}};

constexpr std::array<SpriteCollisionPartView, 7> particle_parts{{
  SpriteCollisionPartView{.points = particle_part_0_points.data(), .point_count = particle_part_0_points.size()},
  SpriteCollisionPartView{.points = particle_part_1_points.data(), .point_count = particle_part_1_points.size()},
  SpriteCollisionPartView{.points = particle_part_2_points.data(), .point_count = particle_part_2_points.size()},
  SpriteCollisionPartView{.points = particle_part_3_points.data(), .point_count = particle_part_3_points.size()},
  SpriteCollisionPartView{.points = particle_part_4_points.data(), .point_count = particle_part_4_points.size()},
  SpriteCollisionPartView{.points = particle_part_5_points.data(), .point_count = particle_part_5_points.size()},
  SpriteCollisionPartView{.points = particle_part_6_points.data(), .point_count = particle_part_6_points.size()},
}};

}  // namespace

SpriteCollisionShapeView sprite_collision_shape_data(SpriteCollisionShape shape) {
  switch (shape) {
    case SpriteCollisionShape::Ship:
      return {.parts = ship_parts.data(), .part_count = ship_parts.size()};
    case SpriteCollisionShape::Rock:
      return {.parts = rock_parts.data(), .part_count = rock_parts.size()};
    case SpriteCollisionShape::Particle:
      return {.parts = particle_parts.data(), .part_count = particle_parts.size()};
  }
  return {};
}

}  // namespace hyperverse
