#include "test_common.hpp"

#include "hyperverse/asteroid_geometry.hpp"
#include "hyperverse/sprite_frame_builder.hpp"
#include "hyperverse/vertical_slice_seed.hpp"

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
  const hyperverse::AsteroidGeometryTuning tuning{.min_radius_scale = 0.70F, .max_radius_scale = 1.20F};
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
    CHECK(geometry.vertices.size() >= 100U);
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
