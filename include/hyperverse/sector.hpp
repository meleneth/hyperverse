#pragma once

#include "hyperverse/math.hpp"

namespace hyperverse {

struct SectorTuning {
  float width{9000.0F};
  float height{9000.0F};
};

inline constexpr float ReferenceScreenWidthPixels = 3840.0F;
inline constexpr float ReferenceScreenHeightPixels = 2160.0F;
inline constexpr float PixelsPerWorldUnit = 0.35F;
inline constexpr float SectorScreensPerAxis = 9.0F;

[[nodiscard]] SectorTuning default_sector();
[[nodiscard]] SectorTuning sector_from_viewport(float viewport_width_pixels, float viewport_height_pixels, float screens = 9.0F);
[[nodiscard]] float wrap_coordinate(float value, float span);
[[nodiscard]] Vec2 wrap_position(Vec2 position, const SectorTuning& sector);
[[nodiscard]] float wrapped_axis_delta(float from, float to, float span);
[[nodiscard]] Vec2 wrapped_delta(Vec2 from, Vec2 to, const SectorTuning& sector);
[[nodiscard]] float wrapped_distance(Vec2 from, Vec2 to, const SectorTuning& sector);

}  // namespace hyperverse
