#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("target lock acquires nearest asteroid and reports wrapped distance") {
  entt::registry registry;
  const entt::entity far = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(far, hyperverse::AsteroidBody{.position = {.x = 4000.0F, .y = 4500.0F}});
  const entt::entity near = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    near,
    hyperverse::AsteroidBody{.position = {.x = 25.0F, .y = 4500.0F}, .radius = 25.0F, .scan_confidence = 0.62F}
  );

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 700.0F};

  hyperverse::update_target_lock(
    lock,
    registry,
    {.x = 8950.0F, .y = 4500.0F},
    {.x = 100.0F, .y = 0.0F},
    {.target_cycle_requested = true},
    sector,
    tuning
  );

  CHECK(hyperverse::has_locked_target(lock));
  CHECK(lock.target == near);
  CHECK(lock.wrapped_distance == Catch::Approx(75.0F));
  CHECK(lock.relative_position.x == Catch::Approx(75.0F));
  CHECK(lock.relative_velocity.x == Catch::Approx(-100.0F));
  CHECK(lock.closing_speed == Catch::Approx(100.0F));
  CHECK(lock.time_to_contact_seconds == Catch::Approx(0.5F));
  CHECK(lock.scan_confidence == Catch::Approx(0.62F));
}

TEST_CASE("target lock cancels and breaks after release range") {
  entt::registry registry;
  const entt::entity target = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(target, hyperverse::AsteroidBody{.position = {.x = 100.0F, .y = 100.0F}});

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 650.0F};

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {}, {.cancel_requested = true}, sector, tuning);
  CHECK_FALSE(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 1200.0F, .y = 100.0F}, {}, {}, sector, tuning);
  CHECK_FALSE(hyperverse::has_locked_target(lock));
}

TEST_CASE("target lock cycles to another asteroid in range") {
  entt::registry registry;
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(first, hyperverse::AsteroidBody{.position = {.x = 200.0F, .y = 100.0F}});
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(second, hyperverse::AsteroidBody{.position = {.x = 260.0F, .y = 100.0F}});

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 650.0F};

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(lock.target == first);

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(lock.target == second);
  CHECK(lock.wrapped_distance == Catch::Approx(160.0F));
}

TEST_CASE("target lock cycles through HUD tracked target order") {
  entt::registry registry;
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(first, hyperverse::AsteroidBody{.position = {.x = 300.0F, .y = 100.0F}});
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(second, hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}});
  const entt::entity third = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(third, hyperverse::AsteroidBody{.position = {.x = 180.0F, .y = 100.0F}});
  const std::vector<entt::entity> tracked{first, third, second};

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 650.0F};

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning, tracked);
  CHECK(lock.target == first);

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning, tracked);
  CHECK(lock.target == third);

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning, tracked);
  CHECK(lock.target == second);
}

TEST_CASE("enemy target lock cycles through combat radar target order") {
  entt::registry registry;
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    first,
    hyperverse::RaiderShip{.position = {.x = 300.0F, .y = 100.0F}, .role = hyperverse::RaiderRole::Combat}
  );
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    second,
    hyperverse::RaiderShip{.position = {.x = 180.0F, .y = 100.0F}, .role = hyperverse::RaiderRole::Combat, .integrity = 60.0F, .max_integrity = 120.0F}
  );
  const entt::entity rock = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(rock, hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}});
  const std::vector<entt::entity> tracked{first, second, rock};

  hyperverse::EnemyTargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};

  hyperverse::update_enemy_target_lock(
    lock,
    registry,
    {.x = 100.0F, .y = 100.0F},
    {},
    {.enemy_target_cycle_requested = true},
    sector,
    {.lock_range = 500.0F, .release_range = 650.0F},
    tracked
  );
  CHECK(hyperverse::has_locked_enemy(lock));
  CHECK(lock.target == first);

  hyperverse::update_enemy_target_lock(
    lock,
    registry,
    {.x = 100.0F, .y = 100.0F},
    {},
    {.enemy_target_cycle_requested = true},
    sector,
    {.lock_range = 500.0F, .release_range = 650.0F},
    tracked
  );
  CHECK(lock.target == second);
  CHECK(lock.integrity_fraction == Catch::Approx(0.5F));
}

TEST_CASE("clear targets request unlocks asteroid and enemy locks") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(asteroid, hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}});
  const entt::entity enemy = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    enemy,
    hyperverse::RaiderShip{.position = {.x = 180.0F, .y = 100.0F}, .role = hyperverse::RaiderRole::Combat}
  );

  hyperverse::TargetLockModel asteroid_lock{.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid};
  hyperverse::EnemyTargetLockModel enemy_lock{.phase = hyperverse::TargetLockPhase::Locked, .target = enemy};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};

  hyperverse::update_target_lock(asteroid_lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.clear_targets_requested = true}, sector);
  hyperverse::update_enemy_target_lock(enemy_lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.clear_targets_requested = true}, sector);

  CHECK_FALSE(hyperverse::has_locked_target(asteroid_lock));
  CHECK_FALSE(hyperverse::has_locked_enemy(enemy_lock));
}

TEST_CASE("asteroid mass initializes from radius and is reduced by extraction") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidMass>(asteroid, hyperverse::asteroid_mass_from_radius(240.0F));

  const float extracted = hyperverse::extract_asteroid_mass(registry, asteroid, 180.0F);

  const hyperverse::AsteroidMass& mass = registry.get<hyperverse::AsteroidMass>(asteroid);
  CHECK(extracted == Catch::Approx(180.0F));
  CHECK(mass.initial_mass == Catch::Approx(720.0F));
  CHECK(mass.remaining_mass == Catch::Approx(540.0F));
}

TEST_CASE("asteroid motion is integrated through the physics step") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{
      .position = {.x = 100.0F, .y = 100.0F},
      .velocity = {.x = 25.0F, .y = 0.0F},
      .radius = 40.0F,
      .base_radius = 40.0F,
      .angular_velocity = 2.0F,
    }
  );

  hyperverse::update_asteroid_motion(account, {.width = 9000.0F, .height = 9000.0F}, 1.0F);

  const hyperverse::AsteroidBody& moved = account.registry().get<hyperverse::AsteroidBody>(asteroid);
  CHECK(moved.position.x == Catch::Approx(125.0F));
  CHECK(moved.position.y == Catch::Approx(100.0F));
  CHECK(moved.rotation_radians == Catch::Approx(2.0F));
}

TEST_CASE("asteroid motion wraps through sector edges during physics integration") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{
      .position = {.x = 8990.0F, .y = 100.0F},
      .velocity = {.x = 25.0F, .y = 0.0F},
      .radius = 40.0F,
      .base_radius = 40.0F,
    }
  );

  hyperverse::update_asteroid_motion(account, {.width = 9000.0F, .height = 9000.0F}, 1.0F);

  const hyperverse::AsteroidBody& moved = account.registry().get<hyperverse::AsteroidBody>(asteroid);
  CHECK(moved.position.x == Catch::Approx(15.0F).margin(0.02F));
  CHECK(moved.position.y == Catch::Approx(100.0F));
}
