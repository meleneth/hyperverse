#include "hyperverse/sprite_collision_shape.hpp"

#include <algorithm>
#include <cmath>

namespace hyperverse {
namespace {

[[nodiscard]] bool is_solid(const SpriteAlphaMask& mask, std::uint32_t x, std::uint32_t y, std::uint8_t alpha_threshold) {
  const std::size_t index = ((static_cast<std::size_t>(y) * mask.width) + x) * 4U;
  return index + 3U < mask.rgba.size() && mask.rgba[index + 3U] >= alpha_threshold;
}

[[nodiscard]] bool is_boundary(const SpriteAlphaMask& mask, std::uint32_t x, std::uint32_t y, std::uint8_t alpha_threshold) {
  if (!is_solid(mask, x, y, alpha_threshold)) {
    return false;
  }
  if (x == 0U || y == 0U || x + 1U >= mask.width || y + 1U >= mask.height) {
    return true;
  }
  return !is_solid(mask, x - 1U, y, alpha_threshold) || !is_solid(mask, x + 1U, y, alpha_threshold) ||
         !is_solid(mask, x, y - 1U, alpha_threshold) || !is_solid(mask, x, y + 1U, alpha_threshold);
}

[[nodiscard]] Vec2 normalized_pixel_center(const SpriteAlphaMask& mask, std::uint32_t x, std::uint32_t y) {
  const float scale = 2.0F / static_cast<float>(std::max(mask.width, mask.height));
  return {
    .x = ((static_cast<float>(x) + 0.5F) - (static_cast<float>(mask.width) * 0.5F)) * scale,
    .y = ((static_cast<float>(mask.height) * 0.5F) - (static_cast<float>(y) + 0.5F)) * scale,
  };
}

[[nodiscard]] float cross(Vec2 origin, Vec2 a, Vec2 b) {
  return ((a.x - origin.x) * (b.y - origin.y)) - ((a.y - origin.y) * (b.x - origin.x));
}

void append_hull_half(std::vector<Vec2>& hull, const std::vector<Vec2>& points) {
  for (Vec2 point : points) {
    while (hull.size() >= 2U && cross(hull[hull.size() - 2U], hull.back(), point) <= 0.0F) {
      hull.pop_back();
    }
    hull.push_back(point);
  }
}

}  // namespace

SpriteSilhouette extract_sprite_silhouette(const SpriteAlphaMask& mask, std::uint8_t alpha_threshold) {
  if (mask.width == 0U || mask.height == 0U || mask.rgba.size() < static_cast<std::size_t>(mask.width) * mask.height * 4U) {
    return {};
  }

  std::vector<Vec2> points;
  for (std::uint32_t y = 0; y < mask.height; ++y) {
    for (std::uint32_t x = 0; x < mask.width; ++x) {
      if (is_boundary(mask, x, y, alpha_threshold)) {
        points.push_back(normalized_pixel_center(mask, x, y));
      }
    }
  }

  if (points.size() <= 2U) {
    return {.hull = points};
  }

  std::sort(points.begin(), points.end(), [](Vec2 lhs, Vec2 rhs) {
    if (std::abs(lhs.x - rhs.x) > 0.0001F) {
      return lhs.x < rhs.x;
    }
    return lhs.y < rhs.y;
  });
  points.erase(
    std::unique(points.begin(), points.end(), [](Vec2 lhs, Vec2 rhs) {
      return std::abs(lhs.x - rhs.x) <= 0.0001F && std::abs(lhs.y - rhs.y) <= 0.0001F;
    }),
    points.end()
  );

  std::vector<Vec2> lower;
  append_hull_half(lower, points);

  std::vector<Vec2> reversed = points;
  std::reverse(reversed.begin(), reversed.end());
  std::vector<Vec2> upper;
  append_hull_half(upper, reversed);

  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return {.hull = lower};
}

}  // namespace hyperverse
