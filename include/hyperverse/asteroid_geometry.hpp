#pragma once

#include <cstdint>
#include <vector>

namespace hyperverse {

struct Vec3 {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

struct AsteroidMeshVertex {
  Vec3 position{};
  float r{0.55F};
  float g{0.52F};
  float b{0.48F};
};

struct AsteroidMeshTriangle {
  std::uint16_t a{};
  std::uint16_t b{};
  std::uint16_t c{};
};

struct AsteroidGeometry {
  std::uint32_t seed{};
  Vec3 tumble_angles{};
  Vec3 tumble_velocity{};
  std::vector<AsteroidMeshVertex> vertices{};
  std::vector<AsteroidMeshTriangle> triangles{};
};

struct AsteroidGeometryTuning {
  int face_subdivisions{3};
  float min_radius_scale{0.34F};
  float max_radius_scale{0.62F};
  float chip_strength{0.18F};
};

[[nodiscard]] AsteroidGeometry generate_asteroid_geometry(std::uint32_t seed, float radius, const AsteroidGeometryTuning& tuning = {});
[[nodiscard]] std::vector<AsteroidGeometry> fracture_asteroid_geometry(
  const AsteroidGeometry& parent,
  Vec3 impact_direction,
  int pieces,
  float child_radius
);
void update_asteroid_tumble(AsteroidGeometry& geometry, float dt_seconds);

}  // namespace hyperverse
