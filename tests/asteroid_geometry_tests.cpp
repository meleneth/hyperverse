#include "test_common.hpp"

#include "hyperverse/asteroid_geometry.hpp"
#include "hyperverse/sprite_frame_builder.hpp"
#include "hyperverse/vertical_slice_seed.hpp"

#include <algorithm>
#include <cmath>

TEST_CASE("asteroid geometry generation is deterministic by seed") {
  const hyperverse::AsteroidGeometry first = hyperverse::generate_asteroid_geometry(42U, 180.0F);
  const hyperverse::AsteroidGeometry second = hyperverse::generate_asteroid_geometry(42U, 180.0F);
  const hyperverse::AsteroidGeometry other = hyperverse::generate_asteroid_geometry(43U, 180.0F);

  REQUIRE(first.vertices.size() == second.vertices.size());
  REQUIRE(first.triangles.size() == second.triangles.size());
  CHECK(first.vertices.front().position.x == Catch::Approx(second.vertices.front().position.x));
  CHECK(first.vertices.front().position.y == Catch::Approx(second.vertices.front().position.y));
  CHECK(first.vertices.front().position.z == Catch::Approx(second.vertices.front().position.z));
  CHECK(first.vertices.front().position.x != Catch::Approx(other.vertices.front().position.x));
}

TEST_CASE("asteroid geometry stays inside configured radius envelope") {
  constexpr float radius = 240.0F;
  const hyperverse::AsteroidGeometryTuning tuning{.min_radius_scale = 0.32F, .max_radius_scale = 0.64F};
  const hyperverse::AsteroidGeometry geometry = hyperverse::generate_asteroid_geometry(99U, radius, tuning);

  REQUIRE_FALSE(geometry.vertices.empty());
  REQUIRE_FALSE(geometry.triangles.empty());
  for (const hyperverse::AsteroidMeshVertex& vertex : geometry.vertices) {
    const float distance = std::sqrt(
      (vertex.position.x * vertex.position.x) +
      (vertex.position.y * vertex.position.y) +
      (vertex.position.z * vertex.position.z)
    );
    CHECK(distance >= Catch::Approx(radius * tuning.min_radius_scale).margin(0.001F));
    CHECK(distance <= Catch::Approx(radius * tuning.max_radius_scale).margin(0.001F));
  }
}

TEST_CASE("asteroid geometry has a chipped non-round silhouette") {
  constexpr float radius = 260.0F;
  const hyperverse::AsteroidGeometry geometry = hyperverse::generate_asteroid_geometry(177U, radius);

  float min_distance = std::numeric_limits<float>::max();
  float max_distance = 0.0F;
  for (const hyperverse::AsteroidMeshVertex& vertex : geometry.vertices) {
    const float distance = std::sqrt(
      (vertex.position.x * vertex.position.x) +
      (vertex.position.y * vertex.position.y) +
      (vertex.position.z * vertex.position.z)
    );
    min_distance = std::min(min_distance, distance);
    max_distance = std::max(max_distance, distance);
  }

  CHECK(max_distance - min_distance >= radius * 0.16F);
}

TEST_CASE("asteroid geometry visual radius fits inside radar bounds") {
  constexpr float radius = 300.0F;
  const hyperverse::AsteroidGeometry geometry = hyperverse::generate_asteroid_geometry(211U, radius);

  float max_xy_radius = 0.0F;
  for (const hyperverse::AsteroidMeshVertex& vertex : geometry.vertices) {
    max_xy_radius = std::max(max_xy_radius, std::sqrt((vertex.position.x * vertex.position.x) + (vertex.position.y * vertex.position.y)));
  }

  const float radar_half_width_world = ((radius * 0.48F) + 18.0F) / (2.0F * hyperverse::PixelsPerWorldUnit);
  CHECK(max_xy_radius < radar_half_width_world);
}

TEST_CASE("asteroid tumble advances independently on three axes") {
  hyperverse::AsteroidGeometry geometry = hyperverse::generate_asteroid_geometry(123U, 150.0F);
  geometry.tumble_angles = {};
  geometry.tumble_velocity = {.x = 0.2F, .y = -0.3F, .z = 0.4F};

  hyperverse::update_asteroid_tumble(geometry, 2.0F);

  CHECK(geometry.tumble_angles.x == Catch::Approx(0.4F));
  CHECK(geometry.tumble_angles.y == Catch::Approx(-0.6F));
  CHECK(geometry.tumble_angles.z == Catch::Approx(0.8F));
}

TEST_CASE("vertical slice asteroids receive generated render geometry") {
  hyperverse::test::TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  (void)hyperverse::seed_vertical_slice(account);

  int asteroid_count = 0;
  for (auto [entity, asteroid, geometry] : account.registry().view<hyperverse::AsteroidBody, hyperverse::AsteroidGeometry>().each()) {
    (void)entity;
    (void)asteroid;
    ++asteroid_count;
    CHECK(geometry.vertices.size() >= 90U);
    CHECK(geometry.triangles.size() >= 100U);
  }

  CHECK(asteroid_count >= 24);
}

TEST_CASE("sprite frame renders generated asteroid geometry as triangles") {
  hyperverse::test::TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);

  const hyperverse::SpriteFrame frame = hyperverse::build_sprite_frame(
    account,
    entities.player,
    entities.mining_drones,
    entities.raider,
    {},
    {},
    hyperverse::default_sector(),
    1920,
    1080
  );

  CHECK_FALSE(frame.triangles.empty());
}

TEST_CASE("sprite frame asteroid triangles carry procedural surface variation") {
  hyperverse::test::TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::VerticalSliceEntities entities = hyperverse::seed_vertical_slice(account);

  const hyperverse::SpriteFrame frame = hyperverse::build_sprite_frame(
    account,
    entities.player,
    entities.mining_drones,
    entities.raider,
    {},
    {},
    hyperverse::default_sector(),
    1920,
    1080
  );

  REQUIRE_FALSE(frame.triangles.empty());

  bool has_varying_color = false;
  bool has_surface_coordinates = false;
  for (const hyperverse::TriangleDraw& triangle : frame.triangles) {
    has_varying_color = has_varying_color || std::abs(triangle.a.r - triangle.b.r) > 0.001F || std::abs(triangle.b.r - triangle.c.r) > 0.001F ||
                        std::abs(triangle.a.g - triangle.b.g) > 0.001F || std::abs(triangle.b.g - triangle.c.g) > 0.001F;
    has_surface_coordinates = has_surface_coordinates || std::abs(triangle.a.u - triangle.b.u) > 0.001F || std::abs(triangle.b.v - triangle.c.v) > 0.001F;
  }

  CHECK(has_varying_color);
  CHECK(has_surface_coordinates);
}
