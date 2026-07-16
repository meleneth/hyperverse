#include "test_common.hpp"

TEST_CASE("harpoon latches to a locked asteroid in range") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 100.0F}, .velocity = {.x = 300.0F, .y = 0.0F}}
  );
  hyperverse::HarpoonModel harpoon{};
  hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::HarpoonHudSnapshot hud = hyperverse::update_harpoon(
    harpoon,
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid, .wrapped_distance = 400.0F},
    ship,
    {.harpoon_requested = true},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F
  );

  CHECK(hud.latched);
  CHECK(harpoon.phase == hyperverse::HarpoonPhase::Latched);
  CHECK(harpoon.target == asteroid);
}

TEST_CASE("latched harpoon bleeds asteroid velocity toward ship velocity") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 100.0F}, .velocity = {.x = 300.0F, .y = 0.0F}}
  );
  hyperverse::HarpoonModel harpoon{.phase = hyperverse::HarpoonPhase::Latched, .target = asteroid};
  hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}, .velocity = {.x = 0.0F, .y = 0.0F}};

  (void)hyperverse::update_harpoon(
    harpoon,
    registry,
    {},
    ship,
    {},
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.asteroid_brake_per_second = 100.0F, .ship_pull_per_second = 0.0F}
  );

  CHECK(registry.get<hyperverse::AsteroidBody>(asteroid).velocity.x == Catch::Approx(250.0F));
}

TEST_CASE("latched spinning asteroid can hurl the ship") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{
      .position = {.x = 500.0F, .y = 100.0F},
      .velocity = {.x = 0.0F, .y = 0.0F},
      .radius = 100.0F,
      .angular_velocity = 4.0F,
    }
  );
  hyperverse::HarpoonModel harpoon{.phase = hyperverse::HarpoonPhase::Latched, .target = asteroid};
  hyperverse::ShipMotion ship{.position = {.x = 600.0F, .y = 100.0F}};

  (void)hyperverse::update_harpoon(
    harpoon,
    registry,
    {},
    ship,
    {},
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.asteroid_brake_per_second = 0.0F, .ship_pull_per_second = 200.0F}
  );

  CHECK(hyperverse::length(ship.velocity) > 0.0F);
  CHECK(ship.velocity.y > 0.0F);
}

TEST_CASE("boosting detaches a latched harpoon") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(asteroid, hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 100.0F}});
  hyperverse::HarpoonModel harpoon{.phase = hyperverse::HarpoonPhase::Latched, .target = asteroid};
  hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::HarpoonHudSnapshot hud = hyperverse::update_harpoon(
    harpoon,
    registry,
    {},
    ship,
    {.boost_requested = true},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F
  );

  CHECK_FALSE(hud.latched);
  CHECK(harpoon.phase == hyperverse::HarpoonPhase::Ready);
}

TEST_CASE("harpoon releases when the target leaves tether range") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(asteroid, hyperverse::AsteroidBody{.position = {.x = 3000.0F, .y = 100.0F}});
  hyperverse::HarpoonModel harpoon{.phase = hyperverse::HarpoonPhase::Latched, .target = asteroid};
  hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::HarpoonHudSnapshot hud = hyperverse::update_harpoon(
    harpoon,
    registry,
    {},
    ship,
    {},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    {.release_range = 1200.0F}
  );

  CHECK_FALSE(hud.latched);
  CHECK(harpoon.phase == hyperverse::HarpoonPhase::Ready);
}
