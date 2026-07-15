#pragma once

#include "hyperverse/math.hpp"

namespace hyperverse {

struct SectorTuning {
  float width{9000.0F};
  float height{9000.0F};
};

[[nodiscard]] float wrap_coordinate(float value, float span);
[[nodiscard]] Vec2 wrap_position(Vec2 position, const SectorTuning& sector);
[[nodiscard]] float wrapped_axis_delta(float from, float to, float span);
[[nodiscard]] Vec2 wrapped_delta(Vec2 from, Vec2 to, const SectorTuning& sector);
[[nodiscard]] float wrapped_distance(Vec2 from, Vec2 to, const SectorTuning& sector);

}  // namespace hyperverse
