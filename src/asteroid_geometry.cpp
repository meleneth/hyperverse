#include "hyperverse/asteroid_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

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

[[nodiscard]] Vec3 operator+(Vec3 lhs, Vec3 rhs) {
  return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y, .z = lhs.z + rhs.z};
}

[[nodiscard]] Vec3 operator-(Vec3 lhs, Vec3 rhs) {
  return {.x = lhs.x - rhs.x, .y = lhs.y - rhs.y, .z = lhs.z - rhs.z};
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

[[nodiscard]] Vec3 cross(Vec3 lhs, Vec3 rhs) {
  return {
    .x = (lhs.y * rhs.z) - (lhs.z * rhs.y),
    .y = (lhs.z * rhs.x) - (lhs.x * rhs.z),
    .z = (lhs.x * rhs.y) - (lhs.y * rhs.x),
  };
}

[[nodiscard]] Vec3 perpendicular_axis(Vec3 value) {
  const Vec3 reference = std::abs(value.x) < 0.75F ? Vec3{.x = 1.0F, .y = 0.0F, .z = 0.0F} : Vec3{.x = 0.0F, .y = 1.0F, .z = 0.0F};
  return normalize_or_zero(cross(value, reference));
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

[[nodiscard]] float chip_distance(Vec3 direction, const std::vector<Vec3>& chips, const std::vector<float>& depths) {
  float distance = 0.0F;
  for (std::size_t index = 0; index < chips.size(); ++index) {
    const float plane_distance = std::max(0.0F, dot(direction, chips[index]) - 0.42F);
    distance += depths[index] * plane_distance;
  }
  return distance;
}

[[nodiscard]] float fracture_angle(Vec3 point, Vec3 impact, Vec3 tangent, Vec3 bitangent) {
  const Vec3 radial = normalize_or_zero(point);
  const float forward = dot(radial, impact);
  const float side = dot(radial, tangent) + (dot(radial, bitangent) * 0.35F);
  return std::atan2(side, forward);
}

[[nodiscard]] int fracture_slice(Vec3 point, Vec3 impact, Vec3 tangent, Vec3 bitangent, int pieces) {
  const float angle = fracture_angle(point, impact, tangent, bitangent) + std::numbers::pi_v<float>;
  const float normalized = angle / (std::numbers::pi_v<float> * 2.0F);
  return std::clamp(static_cast<int>(normalized * static_cast<float>(pieces)), 0, pieces - 1);
}

[[nodiscard]] AsteroidMeshVertex scaled_fragment_vertex(const AsteroidMeshVertex& source, Vec3 center, float scale) {
  return {
    .position = (source.position - center) * scale,
    .r = source.r,
    .g = source.g,
    .b = source.b,
  };
}

[[nodiscard]] AsteroidMeshVertex fracture_cap_vertex(Vec3 position, Rng32& rng) {
  const float shade = rng.range(0.30F, 0.50F);
  return {
    .position = position,
    .r = shade * rng.range(0.86F, 1.02F),
    .g = shade * rng.range(0.82F, 0.96F),
    .b = shade * rng.range(0.76F, 0.90F),
  };
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

  std::vector<Vec3> chips;
  std::vector<float> chip_depths;
  chips.reserve(10);
  chip_depths.reserve(10);
  for (int index = 0; index < 10; ++index) {
    chips.push_back(normalize_or_zero({.x = rng.range(-1.0F, 1.0F), .y = rng.range(-1.0F, 1.0F), .z = rng.range(-1.0F, 1.0F)}));
    chip_depths.push_back(rng.range(0.10F, tuning.chip_strength));
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
        const float lumpy_radius = 0.54F + (directional_noise(direction, lobes, weights) * 0.62F);
        const float chipped_radius = lumpy_radius - chip_distance(direction, chips, chip_depths);
        const float distortion = std::clamp(chipped_radius, tuning.min_radius_scale, tuning.max_radius_scale);
        Vec3 surface = direction * std::clamp(radius * distortion, min_radius, max_radius);
        for (std::size_t chip_index = 0; chip_index < chips.size(); ++chip_index) {
          const float excess = dot(surface, chips[chip_index]) - (radius * (0.32F + chip_depths[chip_index]));
          if (excess > 0.0F) {
            surface = surface - (chips[chip_index] * (excess * 0.82F));
          }
        }
        const float shade = std::clamp(0.58F + ((distortion - 0.48F) * 1.1F) + rng.range(-0.10F, 0.10F), 0.26F, 0.86F);
        geometry.vertices.push_back(
          AsteroidMeshVertex{
            .position = surface,
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

std::vector<AsteroidGeometry> fracture_asteroid_geometry(
  const AsteroidGeometry& parent,
  Vec3 impact_direction,
  int pieces,
  float child_radius
) {
  if (pieces < 2 || parent.vertices.empty() || parent.triangles.empty()) {
    return {};
  }

  const int fragment_count = std::clamp(pieces, 2, 6);
  const Vec3 impact = normalize_or_zero(length(impact_direction) > 0.0F ? impact_direction : Vec3{.x = 1.0F, .y = 0.0F, .z = 0.0F});
  const Vec3 tangent = perpendicular_axis(impact);
  const Vec3 bitangent = normalize_or_zero(cross(impact, tangent));
  const float source_radius = std::max(1.0F, child_radius * std::sqrt(static_cast<float>(fragment_count)));
  const float scale = child_radius / source_radius;

  std::vector<AsteroidGeometry> fragments(static_cast<std::size_t>(fragment_count));
  std::vector<Vec3> centers(static_cast<std::size_t>(fragment_count));
  std::vector<int> center_counts(static_cast<std::size_t>(fragment_count), 0);

  for (const AsteroidMeshTriangle& triangle : parent.triangles) {
    const Vec3 center = (parent.vertices[triangle.a].position + parent.vertices[triangle.b].position + parent.vertices[triangle.c].position) * (1.0F / 3.0F);
    const int slice = fracture_slice(center, impact, tangent, bitangent, fragment_count);
    centers[static_cast<std::size_t>(slice)] = centers[static_cast<std::size_t>(slice)] + center;
    ++center_counts[static_cast<std::size_t>(slice)];
  }

  for (int index = 0; index < fragment_count; ++index) {
    Vec3 center{};
    if (center_counts[static_cast<std::size_t>(index)] > 0) {
      center = centers[static_cast<std::size_t>(index)] * (1.0F / static_cast<float>(center_counts[static_cast<std::size_t>(index)]));
    }
    centers[static_cast<std::size_t>(index)] = center;
  }

  for (int index = 0; index < fragment_count; ++index) {
    AsteroidGeometry& fragment = fragments[static_cast<std::size_t>(index)];
    fragment.seed = parent.seed ^ (0x9E3779B9U * static_cast<std::uint32_t>(index + 1));
    fragment.tumble_angles = parent.tumble_angles;
    const float spin_sign = parent.tumble_velocity.z < 0.0F ? -1.0F : 1.0F;
    fragment.tumble_velocity = {
      .x = parent.tumble_velocity.x + (spin_sign * (0.05F + 0.02F * static_cast<float>(index))),
      .y = parent.tumble_velocity.y + (spin_sign * (0.03F + 0.015F * static_cast<float>(index))),
      .z = parent.tumble_velocity.z + (spin_sign * (0.08F + 0.025F * static_cast<float>(index))),
    };
  }

  for (const AsteroidMeshTriangle& triangle : parent.triangles) {
    const Vec3 center = (parent.vertices[triangle.a].position + parent.vertices[triangle.b].position + parent.vertices[triangle.c].position) * (1.0F / 3.0F);
    const int slice = fracture_slice(center, impact, tangent, bitangent, fragment_count);
    AsteroidGeometry& fragment = fragments[static_cast<std::size_t>(slice)];
    const Vec3 fragment_center = centers[static_cast<std::size_t>(slice)];
    if (fragment.vertices.size() + 3U > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
      continue;
    }
    const std::uint16_t base = static_cast<std::uint16_t>(fragment.vertices.size());
    fragment.vertices.push_back(scaled_fragment_vertex(parent.vertices[triangle.a], fragment_center, scale));
    fragment.vertices.push_back(scaled_fragment_vertex(parent.vertices[triangle.b], fragment_center, scale));
    fragment.vertices.push_back(scaled_fragment_vertex(parent.vertices[triangle.c], fragment_center, scale));
    fragment.triangles.push_back({.a = base, .b = static_cast<std::uint16_t>(base + 1U), .c = static_cast<std::uint16_t>(base + 2U)});
  }

  for (int index = 0; index < fragment_count; ++index) {
    AsteroidGeometry& fragment = fragments[static_cast<std::size_t>(index)];
    if (fragment.vertices.empty()) {
      fragment = generate_asteroid_geometry(parent.seed ^ (0x51ED1234U + static_cast<std::uint32_t>(index)), child_radius);
      continue;
    }

    Rng32 rng{fragment.seed};
    const std::uint16_t center_index = static_cast<std::uint16_t>(fragment.vertices.size());
    const Vec3 cap_center = normalize_or_zero(centers[static_cast<std::size_t>(index)] * -1.0F) * (child_radius * rng.range(0.10F, 0.24F));
    fragment.vertices.push_back(fracture_cap_vertex(cap_center, rng));
    const int cap_points = std::clamp(static_cast<int>(fragment.vertices.size() / 7U), 5, 12);
    const Vec3 cap_normal = normalize_or_zero(cap_center * -1.0F);
    const Vec3 cap_tangent = perpendicular_axis(cap_normal);
    const Vec3 cap_bitangent = normalize_or_zero(cross(cap_normal, cap_tangent));
    const float cap_radius = child_radius * rng.range(0.28F, 0.48F);
    std::vector<std::uint16_t> ring;
    ring.reserve(static_cast<std::size_t>(cap_points));
    for (int point = 0; point < cap_points; ++point) {
      const float angle = (static_cast<float>(point) / static_cast<float>(cap_points)) * std::numbers::pi_v<float> * 2.0F;
      const float rough = rng.range(0.72F, 1.16F);
      const Vec3 position = cap_center + (cap_tangent * (std::cos(angle) * cap_radius * rough)) + (cap_bitangent * (std::sin(angle) * cap_radius * rough));
      ring.push_back(static_cast<std::uint16_t>(fragment.vertices.size()));
      fragment.vertices.push_back(fracture_cap_vertex(position, rng));
    }
    for (int point = 0; point < cap_points; ++point) {
      fragment.triangles.push_back(
        {
          .a = center_index,
          .b = ring[static_cast<std::size_t>(point)],
          .c = ring[static_cast<std::size_t>((point + 1) % cap_points)],
        }
      );
    }
  }

  return fragments;
}

void update_asteroid_tumble(AsteroidGeometry& geometry, float dt_seconds) {
  constexpr float full_turn = std::numbers::pi_v<float> * 2.0F;
  const float dt = std::max(0.0F, dt_seconds);
  geometry.tumble_angles.x = std::fmod(geometry.tumble_angles.x + (geometry.tumble_velocity.x * dt), full_turn);
  geometry.tumble_angles.y = std::fmod(geometry.tumble_angles.y + (geometry.tumble_velocity.y * dt), full_turn);
  geometry.tumble_angles.z = std::fmod(geometry.tumble_angles.z + (geometry.tumble_velocity.z * dt), full_turn);
}

}  // namespace hyperverse
