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
  CHECK(collision.separation == Catch::Approx(50.0F));
  CHECK(collision.impact_speed == Catch::Approx(50.0F));
  CHECK(collision.time_to_contact_seconds == Catch::Approx(1.0F));
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
