#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("mining drone acquires the locked asteroid as its priority") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone drone{.position = {.x = 100.0F, .y = 100.0F}};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 100.0F, .mining_range = 50.0F, .work_standoff = 40.0F}
  );

  CHECK(drone.target == asteroid);
  CHECK(drone.phase == hyperverse::MiningDronePhase::Travelling);
  CHECK(drone.position.x == Catch::Approx(150.0F));
  CHECK(hud.target == asteroid);
}

TEST_CASE("mining drone extracts material when in range") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone drone{.position = {.x = 200.0F, .y = 100.0F}, .target = asteroid};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    2.0F,
    {.mining_range = 50.0F, .work_standoff = 40.0F, .arrival_tolerance = 50.0F, .integrity_damage_per_second = 4.0F, .extraction_per_second = 3.0F}
  );

  const hyperverse::MiningResource& resource = registry.get<hyperverse::MiningResource>(asteroid);
  CHECK(drone.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(hud.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(resource.integrity == Catch::Approx(92.0F));
  CHECK(resource.extracted_mass == Catch::Approx(6.0F));
  CHECK(drone.extracted_mass == Catch::Approx(6.0F));
}

TEST_CASE("mining drones use separate work angles around an asteroid") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 500.0F}, .radius = 80.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone first{.position = {.x = 500.0F, .y = 500.0F}, .target = asteroid, .work_angle_radians = 0.0F};
  hyperverse::MiningDrone second{.position = {.x = 500.0F, .y = 500.0F}, .target = asteroid, .work_angle_radians = 3.1415926F};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  (void)hyperverse::update_mining_drone(first, registry, {}, ship, {.width = 9000.0F, .height = 9000.0F}, 0.5F, {.max_speed = 100.0F, .work_standoff = 120.0F});
  (void)hyperverse::update_mining_drone(second, registry, {}, ship, {.width = 9000.0F, .height = 9000.0F}, 0.5F, {.max_speed = 100.0F, .work_standoff = 120.0F});

  CHECK(first.velocity.x > 0.0F);
  CHECK(second.velocity.x < 0.0F);
}

TEST_CASE("idle mining drones form up behind the player ship") {
  entt::registry registry;
  hyperverse::MiningDrone drone{.position = {.x = 500.0F, .y = 500.0F}, .work_angle_radians = 0.0F};
  const hyperverse::ShipMotion ship{.position = {.x = 500.0F, .y = 500.0F}, .facing_radians = 0.0F};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.max_speed = 100.0F, .formation_trail_distance = 260.0F, .formation_spread = 0.0F}
  );

  CHECK(drone.phase == hyperverse::MiningDronePhase::Idle);
  CHECK(drone.velocity.x < 0.0F);
  CHECK(drone.position.x < 500.0F);
  CHECK_FALSE(registry.valid(hud.target));
  CHECK(hud.target_distance > 0.0F);
}
