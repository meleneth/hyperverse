#pragma once

#include <cmath>

namespace hyperverse {

struct Vec2 {
  float x{0.0F};
  float y{0.0F};

  [[nodiscard]] constexpr Vec2 operator+(Vec2 rhs) const {
    return {.x = x + rhs.x, .y = y + rhs.y};
  }

  [[nodiscard]] constexpr Vec2 operator-(Vec2 rhs) const {
    return {.x = x - rhs.x, .y = y - rhs.y};
  }

  [[nodiscard]] constexpr Vec2 operator*(float scalar) const {
    return {.x = x * scalar, .y = y * scalar};
  }

  constexpr Vec2& operator+=(Vec2 rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  constexpr Vec2& operator-=(Vec2 rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }
};

[[nodiscard]] inline float length(Vec2 value) {
  return std::sqrt((value.x * value.x) + (value.y * value.y));
}

[[nodiscard]] inline Vec2 normalize_or_zero(Vec2 value) {
  const float magnitude = length(value);
  if (magnitude <= 0.0001F) {
    return {};
  }
  return {.x = value.x / magnitude, .y = value.y / magnitude};
}

[[nodiscard]] inline Vec2 clamp_length(Vec2 value, float maximum) {
  const float magnitude = length(value);
  if (magnitude <= maximum || magnitude <= 0.0001F) {
    return value;
  }
  return normalize_or_zero(value) * maximum;
}

}  // namespace hyperverse
