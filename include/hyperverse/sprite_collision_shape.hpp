#pragma once

#include "hyperverse/math.hpp"

#include <cstdint>
#include <vector>

namespace hyperverse {

enum class SpriteCollisionShape {
  Ship,
  Rock,
  Particle,
};

struct SpriteAlphaMask {
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::vector<std::uint8_t> rgba{};
};

struct SpriteSilhouette {
  std::vector<Vec2> hull{};
};

[[nodiscard]] SpriteSilhouette extract_sprite_silhouette(const SpriteAlphaMask& mask, std::uint8_t alpha_threshold = 16U);

}  // namespace hyperverse
