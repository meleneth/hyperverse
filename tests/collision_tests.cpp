#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("collision prediction warns before ship reaches asteroid") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 200.0F, .y = 100.0F}, .radius = 40.0F}
  );

  const hyperverse::ShipMotion ship{
    .position = {.x = 100.0F, .y = 100.0F},
    .velocity = {.x = 50.0F, .y = 0.0F},
  };
  const hyperverse::CollisionHudSnapshot collision = hyperverse::predict_ship_asteroid_collision(
    ship,
    registry,
    {.width = 9000.0F, .height = 9000.0F},
    {.ship_radius = 10.0F, .warning_seconds = 2.0F}
  );

  CHECK_FALSE(collision.contact);
  CHECK(collision.warning);
  CHECK(collision.asteroid == asteroid);
  CHECK(collision.separation > 0.0F);
  CHECK(collision.separation < 100.0F);
  CHECK(collision.impact_speed == Catch::Approx(50.0F));
  CHECK(collision.time_to_contact_seconds == Catch::Approx(collision.separation / collision.impact_speed));
}

TEST_CASE("swept collision catches fast ship crossing an asteroid between ticks") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 200.0F, .y = 100.0F}, .radius = 12.0F}
  );

  const hyperverse::ShipMotion ship{
    .position = {.x = 100.0F, .y = 100.0F},
    .velocity = {.x = 10000.0F, .y = 0.0F},
  };
  const hyperverse::CollisionHudSnapshot collision = hyperverse::predict_ship_asteroid_collision(
    ship,
    registry,
    {.width = 9000.0F, .height = 9000.0F},
    {.ship_radius = 3.0F, .warning_seconds = 0.016F}
  );

  CHECK(collision.warning);
  CHECK(collision.asteroid == asteroid);
  CHECK(collision.time_to_contact_seconds < 0.016F);
  CHECK(collision.swept_checks == 1U);
}

TEST_CASE("collision prediction scales swept checks from speed and zoom tuning") {
  entt::registry registry;
  for (int index = 0; index < 20; ++index) {
    const entt::entity asteroid = registry.create();
    registry.emplace<hyperverse::AsteroidBody>(
      asteroid,
      hyperverse::AsteroidBody{
        .position = {.x = 200.0F + (static_cast<float>(index) * 20.0F), .y = 100.0F},
        .radius = 12.0F,
      }
    );
  }

  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::CollisionProbe base_probe{
    .ship_radius = 3.0F,
    .warning_seconds = 1.0F,
    .view_radius_world = 1000.0F,
    .zoom_scale = 1.0F,
    .min_swept_checks = 2U,
    .max_swept_checks = 10U,
    .speed_checks_per_world_unit = 0.01F,
    .zoom_checks_per_scale = 4.0F,
  };

  const hyperverse::CollisionHudSnapshot slow = hyperverse::predict_ship_asteroid_collision(
    hyperverse::ShipMotion{.position = {.x = 100.0F, .y = 100.0F}, .velocity = {.x = 0.0F, .y = 40.0F}},
    registry,
    sector,
    base_probe
  );
  hyperverse::CollisionProbe zoomed_probe = base_probe;
  zoomed_probe.zoom_scale = 1.0F;
  const hyperverse::CollisionHudSnapshot fast_zoomed = hyperverse::predict_ship_asteroid_collision(
    hyperverse::ShipMotion{.position = {.x = 100.0F, .y = 100.0F}, .velocity = {.x = 400.0F, .y = 0.0F}},
    registry,
    sector,
    zoomed_probe
  );

  CHECK(slow.candidate_count == 7U);
  CHECK(slow.swept_checks == 7U);
  CHECK(fast_zoomed.candidate_count == 10U);
  CHECK(fast_zoomed.swept_checks == 10U);
}

TEST_CASE("collision prediction reports current contact") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 110.0F, .y = 100.0F}, .radius = 40.0F}
  );

  const hyperverse::ShipMotion ship{
    .position = {.x = 100.0F, .y = 100.0F},
    .velocity = {.x = 5.0F, .y = 0.0F},
  };
  const hyperverse::CollisionHudSnapshot collision = hyperverse::predict_ship_asteroid_collision(
    ship,
    registry,
    {.width = 9000.0F, .height = 9000.0F},
    {.ship_radius = 10.0F}
  );

  CHECK(collision.contact);
  CHECK(collision.warning);
  CHECK(collision.separation == Catch::Approx(0.0F));
}

TEST_CASE("asteroid collision uses solid mesh radius instead of logical mining radius") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 167.0F, .y = 100.0F}, .radius = 100.0F}
  );

  const hyperverse::ShipMotion ship{
    .position = {.x = 100.0F, .y = 100.0F},
    .velocity = {},
  };

  const hyperverse::CollisionHudSnapshot collision = hyperverse::predict_ship_asteroid_collision(
    ship,
    registry,
    {.width = 9000.0F, .height = 9000.0F},
    {.ship_radius = 4.0F}
  );

  CHECK_FALSE(collision.contact);
  CHECK_FALSE(collision.warning);
}

TEST_CASE("asteroid physics does not bounce before visible solid radii touch") {
  hyperverse::test::TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};

  const entt::entity left = account.registry().create();
  const entt::entity right = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    left,
    hyperverse::AsteroidBody{.position = {.x = 100.0F, .y = 100.0F}, .velocity = {.x = 20.0F, .y = 0.0F}, .radius = 100.0F}
  );
  account.registry().emplace<hyperverse::AsteroidBody>(
    right,
    hyperverse::AsteroidBody{.position = {.x = 231.0F, .y = 100.0F}, .velocity = {.x = -20.0F, .y = 0.0F}, .radius = 100.0F}
  );

  hyperverse::update_asteroid_motion(account, sector, 0.1F);

  CHECK(account.registry().get<hyperverse::AsteroidBody>(left).velocity.x > 0.0F);
  CHECK(account.registry().get<hyperverse::AsteroidBody>(right).velocity.x < 0.0F);
}
