#include "hyperverse/asteroid_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace hyperverse {
namespace {

struct Rng32 {
  std::uint32_t state{};

  [[nodiscard]] std::uint32_t next_u32() {
    state += 0x9E3779B9U;
    std::uint32_t value = state;
    value = (value ^ (value >> 16U)) * 0x85EBCA6BU;
    value = (value ^ (value >> 13U)) * 0xC2B2AE35U;
    return value ^ (value >> 16U);
  }

  [[nodiscard]] float unit() {
    return static_cast<float>(next_u32() >> 8U) * (1.0F / 16777215.0F);
  }

  [[nodiscard]] float range(float min_value, float max_value) {
    return min_value + ((max_value - min_value) * unit());
  }
};

[[nodiscard]] Vec3 operator*(Vec3 value, float scale) {
  return {.x = value.x * scale, .y = value.y * scale, .z = value.z * scale};
}

[[nodiscard]] float dot(Vec3 lhs, Vec3 rhs) {
  return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] float length(Vec3 value) {
  return std::sqrt(dot(value, value));
}

[[nodiscard]] Vec3 normalize_or_zero(Vec3 value) {
  const float magnitude = length(value);
  return magnitude > 0.0001F ? value * (1.0F / magnitude) : Vec3{};
}

[[nodiscard]] Vec3 face_point(int face, float u, float v) {
  switch (face) {
    case 0:
      return {.x = 1.0F, .y = u, .z = v};
    case 1:
      return {.x = -1.0F, .y = -u, .z = v};
    case 2:
      return {.x = u, .y = 1.0F, .z = v};
    case 3:
      return {.x = -u, .y = -1.0F, .z = v};
    case 4:
      return {.x = u, .y = v, .z = 1.0F};
    default:
      return {.x = u, .y = -v, .z = -1.0F};
  }
}

[[nodiscard]] float directional_noise(Vec3 direction, const std::vector<Vec3>& lobes, const std::vector<float>& weights) {
  float value = 0.0F;
  for (std::size_t index = 0; index < lobes.size(); ++index) {
    const float alignment = std::max(0.0F, dot(direction, lobes[index]));
    value += weights[index] * alignment * alignment;
  }
  return value;
}

}  // namespace

AsteroidGeometry generate_asteroid_geometry(std::uint32_t seed, float radius, const AsteroidGeometryTuning& tuning) {
  Rng32 rng{seed == 0U ? 0xA5311E5DU : seed};
  AsteroidGeometry geometry{.seed = seed};

  geometry.tumble_angles = {
    .x = rng.range(0.0F, std::numbers::pi_v<float> * 2.0F),
    .y = rng.range(0.0F, std::numbers::pi_v<float> * 2.0F),
    .z = rng.range(0.0F, std::numbers::pi_v<float> * 2.0F),
  };
  geometry.tumble_velocity = {
    .x = rng.range(-0.28F, 0.28F),
    .y = rng.range(-0.22F, 0.22F),
    .z = rng.range(-0.34F, 0.34F),
  };

  std::vector<Vec3> lobes;
  std::vector<float> weights;
  lobes.reserve(20);
  weights.reserve(20);
  for (int index = 0; index < 20; ++index) {
    lobes.push_back(normalize_or_zero({.x = rng.range(-1.0F, 1.0F), .y = rng.range(-1.0F, 1.0F), .z = rng.range(-1.0F, 1.0F)}));
    weights.push_back(rng.range(-0.18F, 0.22F));
  }

  const int subdivisions = std::clamp(tuning.face_subdivisions, 1, 8);
  const float step = 2.0F / static_cast<float>(subdivisions);
  const float min_radius = radius * tuning.min_radius_scale;
  const float max_radius = radius * tuning.max_radius_scale;

  geometry.vertices.reserve(static_cast<std::size_t>(6 * (subdivisions + 1) * (subdivisions + 1)));
  geometry.triangles.reserve(static_cast<std::size_t>(6 * subdivisions * subdivisions * 2));

  for (int face = 0; face < 6; ++face) {
    const std::uint16_t base_index = static_cast<std::uint16_t>(geometry.vertices.size());
    for (int y = 0; y <= subdivisions; ++y) {
      for (int x = 0; x <= subdivisions; ++x) {
        const float u = -1.0F + (static_cast<float>(x) * step);
        const float v = -1.0F + (static_cast<float>(y) * step);
        Vec3 direction = normalize_or_zero(face_point(face, u, v));
        const float distortion = std::clamp(1.0F + directional_noise(direction, lobes, weights), tuning.min_radius_scale, tuning.max_radius_scale);
        const float surface_radius = std::clamp(radius * distortion, min_radius, max_radius);
        const float shade = std::clamp(0.58F + ((distortion - 1.0F) * 0.8F) + rng.range(-0.08F, 0.08F), 0.30F, 0.82F);
        geometry.vertices.push_back(
          AsteroidMeshVertex{
            .position = direction * surface_radius,
            .r = shade * rng.range(0.86F, 1.04F),
            .g = shade * rng.range(0.82F, 0.98F),
            .b = shade * rng.range(0.74F, 0.92F),
          }
        );
      }
    }

    const int row_stride = subdivisions + 1;
    for (int y = 0; y < subdivisions; ++y) {
      for (int x = 0; x < subdivisions; ++x) {
        const std::uint16_t a = static_cast<std::uint16_t>(base_index + y * row_stride + x);
        const std::uint16_t b = static_cast<std::uint16_t>(a + 1U);
        const std::uint16_t c = static_cast<std::uint16_t>(a + row_stride);
        const std::uint16_t d = static_cast<std::uint16_t>(c + 1U);
        geometry.triangles.push_back({.a = a, .b = b, .c = d});
        geometry.triangles.push_back({.a = a, .b = d, .c = c});
      }
    }
  }

  return geometry;
}

void update_asteroid_tumble(AsteroidGeometry& geometry, float dt_seconds) {
  constexpr float full_turn = std::numbers::pi_v<float> * 2.0F;
  const float dt = std::max(0.0F, dt_seconds);
  geometry.tumble_angles.x = std::fmod(geometry.tumble_angles.x + (geometry.tumble_velocity.x * dt), full_turn);
  geometry.tumble_angles.y = std::fmod(geometry.tumble_angles.y + (geometry.tumble_velocity.y * dt), full_turn);
  geometry.tumble_angles.z = std::fmod(geometry.tumble_angles.z + (geometry.tumble_velocity.z * dt), full_turn);
}

}  // namespace hyperverse
