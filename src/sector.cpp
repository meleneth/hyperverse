#include "hyperverse/sector.hpp"

#include <cmath>

namespace hyperverse {

float wrap_coordinate(float value, float span) {
  float wrapped = std::fmod(value, span);
  if (wrapped < 0.0F) {
    wrapped += span;
  }
  return wrapped;
}

Vec2 wrap_position(Vec2 position, const SectorTuning& sector) {
  return {.x = wrap_coordinate(position.x, sector.width), .y = wrap_coordinate(position.y, sector.height)};
}

float wrapped_axis_delta(float from, float to, float span) {
  float delta = to - from;
  const float half_span = span * 0.5F;
  if (delta > half_span) {
    delta -= span;
  } else if (delta < -half_span) {
    delta += span;
  }
  return delta;
}

Vec2 wrapped_delta(Vec2 from, Vec2 to, const SectorTuning& sector) {
  return {
    .x = wrapped_axis_delta(from.x, to.x, sector.width),
    .y = wrapped_axis_delta(from.y, to.y, sector.height),
  };
}

float wrapped_distance(Vec2 from, Vec2 to, const SectorTuning& sector) {
  return length(wrapped_delta(from, to, sector));
}

}  // namespace hyperverse
