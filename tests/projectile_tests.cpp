#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

namespace {

[[nodiscard]] std::ptrdiff_t particle_count(entt::registry& registry) {
  return std::distance(registry.view<hyperverse::ParticleShot>().begin(), registry.view<hyperverse::ParticleShot>().end());
}

[[nodiscard]] std::ptrdiff_t missile_count(entt::registry& registry) {
  return std::distance(registry.view<hyperverse::HomingMissile>().begin(), registry.view<hyperverse::HomingMissile>().end());
}

[[nodiscard]] std::ptrdiff_t explosion_count(entt::registry& registry) {
  return std::distance(registry.view<hyperverse::ExplosionBurst>().begin(), registry.view<hyperverse::ExplosionBurst>().end());
}

[[nodiscard]] entt::entity make_player(TestAccountWorld& world, hyperverse::Vec2 position) {
  const entt::entity player = world.registry.create();
  world.registry.emplace<hyperverse::ShipMotion>(player, hyperverse::ShipMotion{.position = position});
  world.registry.emplace<hyperverse::ShipHealth>(player);
  world.registry.emplace<hyperverse::ParticleCannonModel>(player);
  world.registry.emplace<hyperverse::HomingMissileLauncherModel>(player);
  return player;
}

[[nodiscard]] hyperverse::SectorTickCtx tick_context(hyperverse::AccountCtx& account, float dt_seconds) {
  static constexpr hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  return {account, sector, dt_seconds};
}

}  // namespace

TEST_CASE("particle cannon spawns a shot from semantic fire") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 100.0F, .y = 100.0F});
  int fired_events = 0;
  account.event_bus().appendListener(
    hyperverse::DomainEventType::ParticleFired,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::ParticleFired);
      ++fired_events;
    }
  );

  hyperverse::update_player_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(player)},
    {.active = true},
    {.projectile_speed = 100.0F}
  );
  world.registry.get<hyperverse::ShipMotion>(player).position = {.x = 1000.0F, .y = 1000.0F};
  const hyperverse::ParticleCannonHudSnapshot hud = hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player}
  );
  account.event_bus().process();

  CHECK(hud.active_particles == 2);
  CHECK(fired_events == 2);
  CHECK(particle_count(account.registry()) == 2);
}

TEST_CASE("held particle fire uses a four hertz FSM cadence") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 100.0F, .y = 100.0F});

  hyperverse::update_player_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(player)},
    {.active = true},
    {.fire_interval_seconds = 0.25F}
  );
  hyperverse::update_player_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.20F).entity_context(player)},
    {.active = true},
    {.fire_interval_seconds = 0.25F}
  );
  CHECK(particle_count(account.registry()) == 2);

  hyperverse::update_player_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.05F).entity_context(player)},
    {.active = true},
    {.fire_interval_seconds = 0.25F}
  );
  CHECK(particle_count(account.registry()) == 4);
  CHECK(account.registry().get<hyperverse::ParticleCannonModel>(player).phase == hyperverse::ParticleCannonPhase::Cooling);
}

TEST_CASE("particle cannon damages asteroid structure without removing mass") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}, .radius = 80.0F, .base_radius = 80.0F}
  );
  account.registry().emplace<hyperverse::MiningResource>(asteroid);
  account.registry().emplace<hyperverse::AsteroidMass>(asteroid, hyperverse::AsteroidMass{.initial_mass = 80.0F, .remaining_mass = 80.0F});
  hyperverse::ParticleCannonHudSnapshot event_hud{};
  int asteroid_damage_events = 0;
  account.event_bus().appendListener(
    hyperverse::DomainEventType::ParticleImpact,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::ParticleImpact);
      ++event_hud.impacts;
    }
  );
  account.event_bus().appendListener(
    hyperverse::DomainEventType::AsteroidDamaged,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::AsteroidDamaged);
      CHECK(event.subject == asteroid);
      ++asteroid_damage_events;
    }
  );
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{.position = {.x = 110.0F, .y = 100.0F}, .damage = 25.0F, .radius = 10.0F}
  );
  const entt::entity player = make_player(world, {.x = 100.0F, .y = 100.0F});

  const hyperverse::ParticleCannonHudSnapshot hud = hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player}
  );
  account.event_bus().process();

  CHECK(hud.impacts == 0);
  CHECK(event_hud.impacts == 1);
  CHECK(asteroid_damage_events == 1);
  CHECK(account.registry().get<hyperverse::MiningResource>(asteroid).integrity == Catch::Approx(75.0F));
  CHECK(account.registry().get<hyperverse::AsteroidMass>(asteroid).remaining_mass == Catch::Approx(80.0F));
  CHECK(account.registry().get<hyperverse::AsteroidBody>(asteroid).radius == Catch::Approx(80.0F));
  CHECK(particle_count(account.registry()) == 0);
}

TEST_CASE("particle cannon uses swept collision for fast asteroid impacts") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 200.0F, .y = 100.0F}, .radius = 40.0F, .base_radius = 40.0F}
  );
  account.registry().emplace<hyperverse::MiningResource>(asteroid);
  account.registry().emplace<hyperverse::AsteroidMass>(asteroid, hyperverse::AsteroidMass{.initial_mass = 40.0F, .remaining_mass = 40.0F});
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{
      .position = {.x = 100.0F, .y = 100.0F},
      .velocity = {.x = 1400.0F, .y = 0.0F},
      .damage = 25.0F,
      .radius = 10.0F,
    }
  );
  const entt::entity player = make_player(world, {.x = 1000.0F, .y = 1000.0F});

  const hyperverse::ParticleCannonHudSnapshot hud = hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.10F), player}
  );

  CHECK(hud.precise_collision_checks == 1);
  CHECK(account.registry().get<hyperverse::MiningResource>(asteroid).integrity == Catch::Approx(75.0F));
  CHECK(particle_count(account.registry()) == 0);
}

TEST_CASE("particle cannon skips precise collision checks for implausible targets") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  for (int index = 0; index < 12; ++index) {
    const entt::entity asteroid = account.registry().create();
    account.registry().emplace<hyperverse::AsteroidBody>(
      asteroid,
      hyperverse::AsteroidBody{
        .position = {.x = 2000.0F + (static_cast<float>(index) * 120.0F), .y = 3000.0F},
        .radius = 40.0F,
        .base_radius = 40.0F,
      }
    );
    account.registry().emplace<hyperverse::MiningResource>(asteroid);
  }
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{
      .position = {.x = 100.0F, .y = 100.0F},
      .velocity = {.x = 100.0F, .y = 0.0F},
      .damage = 25.0F,
      .radius = 10.0F,
    }
  );
  const entt::entity player = make_player(world, {.x = 1000.0F, .y = 1000.0F});

  const hyperverse::ParticleCannonHudSnapshot hud = hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.10F), player}
  );

  CHECK(hud.active_particles == 1);
  CHECK(hud.precise_collision_checks == 0);
  CHECK(particle_count(account.registry()) == 1);
}

TEST_CASE("kinetic particle impacts can slow an asteroid from the front") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{
      .position = {.x = 120.0F, .y = 100.0F},
      .velocity = {.x = 90.0F, .y = 0.0F},
      .radius = 80.0F,
      .base_radius = 80.0F,
    }
  );
  account.registry().emplace<hyperverse::MiningResource>(asteroid);
  account.registry().emplace<hyperverse::AsteroidMass>(asteroid, hyperverse::AsteroidMass{.initial_mass = 80.0F, .remaining_mass = 80.0F});
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{
      .position = {.x = 110.0F, .y = 100.0F},
      .velocity = {.x = -400.0F, .y = 0.0F},
      .damage = 25.0F,
      .radius = 10.0F,
    }
  );
  const entt::entity player = make_player(world, {.x = 100.0F, .y = 100.0F});

  (void)hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player},
    {.asteroid_kinetic_impulse_scale = 0.20F, .impact_kind = hyperverse::AsteroidImpactKind::Kinetic}
  );

  CHECK(account.registry().get<hyperverse::AsteroidBody>(asteroid).velocity.x < 90.0F);
}

TEST_CASE("glancing kinetic particle impacts spin asteroids") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{
      .position = {.x = 100.0F, .y = 100.0F},
      .radius = 80.0F,
      .base_radius = 80.0F,
    }
  );
  account.registry().emplace<hyperverse::MiningResource>(asteroid);
  account.registry().emplace<hyperverse::AsteroidMass>(asteroid, hyperverse::AsteroidMass{.initial_mass = 80.0F, .remaining_mass = 80.0F});
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{
      .position = {.x = 100.0F, .y = 135.0F},
      .velocity = {.x = 400.0F, .y = 0.0F},
      .damage = 25.0F,
      .radius = 10.0F,
    }
  );
  const entt::entity player = make_player(world, {.x = 1000.0F, .y = 1000.0F});

  (void)hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player},
    {.asteroid_kinetic_impulse_scale = 0.0F, .asteroid_angular_impulse_scale = 1.0F}
  );

  CHECK(account.registry().get<hyperverse::AsteroidBody>(asteroid).angular_velocity < 0.0F);
}

TEST_CASE("player particle shots damage raiders") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity raider_entity = account.registry().create();
  account.registry().emplace<hyperverse::RaiderShip>(
    raider_entity,
    hyperverse::RaiderShip{.position = {.x = 120.0F, .y = 100.0F}, .integrity = 40.0F, .max_integrity = 40.0F}
  );
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{.position = {.x = 120.0F, .y = 100.0F}, .damage = 25.0F, .radius = 10.0F}
  );
  const entt::entity player = make_player(world, {.x = 1000.0F, .y = 1000.0F});

  (void)hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player}
  );

  CHECK(account.registry().get<hyperverse::RaiderShip>(raider_entity).integrity == Catch::Approx(15.0F));
  CHECK(particle_count(account.registry()) == 0);
}

TEST_CASE("raider particle shots damage player shields before armor") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(
    particle,
    hyperverse::ParticleShot{
      .position = {.x = 100.0F, .y = 100.0F},
      .damage = 30.0F,
      .radius = 10.0F,
      .owner = hyperverse::ProjectileOwner::Raider,
    }
  );
  const entt::entity player = make_player(world, {.x = 100.0F, .y = 100.0F});
  world.registry.get<hyperverse::ShipHealth>(player) = {.armor = 100.0F, .shields = 12.0F};

  (void)hyperverse::update_particle_projectiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player}
  );

  const hyperverse::ShipHealth& health = world.registry.get<hyperverse::ShipHealth>(player);
  CHECK(health.shields == Catch::Approx(0.0F));
  CHECK(health.armor == Catch::Approx(82.0F));
  CHECK(particle_count(account.registry()) == 0);
}

TEST_CASE("active raiders fire particle pairs toward the player") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 300.0F, .y = 100.0F});
  const entt::entity raider = world.registry.create();
  world.registry.emplace<hyperverse::RaiderShip>(raider, hyperverse::RaiderShip{.position = {.x = 100.0F, .y = 100.0F}});
  world.registry.emplace<hyperverse::ParticleCannonModel>(raider);

  hyperverse::update_raider_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(raider)},
    tick_context(account, 0.0F).entity_context(player),
    {.active = true},
    {.projectile_speed = 100.0F}
  );

  CHECK(particle_count(account.registry()) == 2);
  for (auto [entity, shot] : account.registry().view<hyperverse::ParticleShot>().each()) {
    (void)entity;
    CHECK(shot.owner == hyperverse::ProjectileOwner::Raider);
    CHECK(shot.velocity.x > 0.0F);
  }
}

TEST_CASE("raider particle cannon uses slower dedicated cadence") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 300.0F, .y = 100.0F});
  const entt::entity raider = world.registry.create();
  world.registry.emplace<hyperverse::RaiderShip>(raider, hyperverse::RaiderShip{.position = {.x = 100.0F, .y = 100.0F}});
  world.registry.emplace<hyperverse::ParticleCannonModel>(raider);
  const hyperverse::ParticleCannonTuning tuning{.projectile_speed = 100.0F, .fire_interval_seconds = 0.25F, .raider_fire_interval_seconds = 0.70F};

  hyperverse::update_raider_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(raider)},
    tick_context(account, 0.0F).entity_context(player),
    {.active = true},
    tuning
  );
  hyperverse::update_raider_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.25F).entity_context(raider)},
    tick_context(account, 0.25F).entity_context(player),
    {.active = true},
    tuning
  );
  CHECK(particle_count(account.registry()) == 2);

  hyperverse::update_raider_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.45F).entity_context(raider)},
    tick_context(account, 0.45F).entity_context(player),
    {.active = true},
    tuning
  );
  CHECK(particle_count(account.registry()) == 4);
}

TEST_CASE("following drones fire past the player at half player cadence") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 200.0F, .y = 100.0F});
  const entt::entity drone = world.registry.create();
  world.registry.emplace<hyperverse::MiningDrone>(
    drone,
    hyperverse::MiningDrone{.position = {.x = 100.0F, .y = 100.0F}, .phase = hyperverse::MiningDronePhase::Idle}
  );
  world.registry.emplace<hyperverse::ParticleCannonModel>(drone);
  const hyperverse::ParticleCannonTuning tuning{
    .projectile_speed = 100.0F,
    .fire_interval_seconds = 0.25F,
    .drone_fire_interval_seconds = 0.50F,
    .drone_player_clearance = 86.0F,
  };

  hyperverse::update_drone_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(drone)},
    tick_context(account, 0.0F).entity_context(player),
    {.aim = {.x = 1.0F, .y = 0.0F}, .active = true},
    tuning
  );
  hyperverse::update_drone_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.25F).entity_context(drone)},
    tick_context(account, 0.25F).entity_context(player),
    {.aim = {.x = 1.0F, .y = 0.0F}, .active = true},
    tuning
  );

  CHECK(particle_count(account.registry()) == 2);
  for (auto [entity, shot] : account.registry().view<hyperverse::ParticleShot>().each()) {
    (void)entity;
    CHECK(shot.owner == hyperverse::ProjectileOwner::Player);
    CHECK(std::abs(shot.position.y - world.registry.get<hyperverse::ShipMotion>(player).position.y) >= tuning.drone_player_clearance);
  }

  hyperverse::update_drone_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.25F).entity_context(drone)},
    tick_context(account, 0.25F).entity_context(player),
    {.aim = {.x = 1.0F, .y = 0.0F}, .active = true},
    tuning
  );
  CHECK(particle_count(account.registry()) == 4);
}

TEST_CASE("working drones do not fire while away from player formation") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 200.0F, .y = 100.0F});
  const entt::entity drone = world.registry.create();
  world.registry.emplace<hyperverse::MiningDrone>(
    drone,
    hyperverse::MiningDrone{.position = {.x = 100.0F, .y = 100.0F}, .phase = hyperverse::MiningDronePhase::Mining}
  );
  world.registry.emplace<hyperverse::ParticleCannonModel>(drone);

  hyperverse::update_drone_particle_cannon(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(drone)},
    tick_context(account, 0.0F).entity_context(player),
    {.aim = {.x = 1.0F, .y = 0.0F}, .active = true}
  );

  CHECK(particle_count(account.registry()) == 0);
}

TEST_CASE("raider particle cannon leads a moving player") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 500.0F, .y = 100.0F});
  world.registry.get<hyperverse::ShipMotion>(player).velocity = {.x = 0.0F, .y = 300.0F};
  const entt::entity raider = world.registry.create();
  world.registry.emplace<hyperverse::RaiderShip>(raider, hyperverse::RaiderShip{.position = {.x = 100.0F, .y = 100.0F}});
  world.registry.emplace<hyperverse::ParticleCannonModel>(raider);

  const std::optional<hyperverse::ParticleCannonFireCommand> command = hyperverse::request_raider_particle_fire(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(raider)},
    tick_context(account, 0.0F).entity_context(player),
    {.active = true},
    {.projectile_speed = 1000.0F}
  );

  REQUIRE(command.has_value());
  CHECK(command->direction.x > 0.0F);
  CHECK(command->direction.y > 0.0F);
}

TEST_CASE("player homing missiles fire as left and right locked-target pair") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 100.0F, .y = 100.0F});
  world.registry.get<hyperverse::ShipMotion>(player).facing_radians = 0.0F;
  const entt::entity raider = world.registry.create();
  world.registry.emplace<hyperverse::RaiderShip>(raider, hyperverse::RaiderShip{.position = {.x = 700.0F, .y = 100.0F}});
  const hyperverse::EnemyTargetLockModel lock{.phase = hyperverse::TargetLockPhase::Locked, .target = raider};
  int fired_events = 0;
  account.event_bus().appendListener(hyperverse::DomainEventType::HomingMissileFired, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.target == raider);
    ++fired_events;
  });

  hyperverse::update_player_homing_missile_launcher(
    hyperverse::WeaponCtx{tick_context(account, 0.0F).entity_context(player)},
    lock,
    {.active = true},
    {.eject_side_offset = 40.0F, .eject_forward_offset = 20.0F}
  );
  account.event_bus().process();

  CHECK(missile_count(account.registry()) == 2);
  CHECK(fired_events == 2);
  bool saw_left = false;
  bool saw_right = false;
  for (auto [entity, missile] : account.registry().view<hyperverse::HomingMissile>().each()) {
    (void)entity;
    CHECK(missile.phase == hyperverse::HomingMissilePhase::Ejected);
    CHECK(missile.target == raider);
    CHECK(missile.position.x == Catch::Approx(120.0F));
    saw_left = saw_left || missile.position.y == Catch::Approx(60.0F);
    saw_right = saw_right || missile.position.y == Catch::Approx(140.0F);
  }
  CHECK(saw_left);
  CHECK(saw_right);
}

TEST_CASE("homing missile coasts before SML ignition then turns toward locked enemy") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 1000.0F, .y = 1000.0F});
  const entt::entity raider = world.registry.create();
  world.registry.emplace<hyperverse::RaiderShip>(raider, hyperverse::RaiderShip{.position = {.x = 500.0F, .y = 500.0F}});
  const entt::entity missile_entity = world.registry.create();
  world.registry.emplace<hyperverse::HomingMissile>(
    missile_entity,
    hyperverse::HomingMissile{
      .position = {.x = 100.0F, .y = 100.0F},
      .velocity = {.x = 300.0F, .y = 0.0F},
      .target = raider,
      .ignition_seconds_remaining = 0.50F,
    }
  );

  (void)hyperverse::update_homing_missiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.49F), player},
    {.turn_responsiveness = 10.0F}
  );
  hyperverse::HomingMissile& coasting = world.registry.get<hyperverse::HomingMissile>(missile_entity);
  CHECK(coasting.phase == hyperverse::HomingMissilePhase::Ejected);
  CHECK(coasting.velocity.y == Catch::Approx(0.0F));

  int ignition_events = 0;
  account.event_bus().appendListener(hyperverse::DomainEventType::HomingMissileIgnited, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.subject == missile_entity);
    ++ignition_events;
  });
  (void)hyperverse::update_homing_missiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.01F), player},
    {.motor_acceleration = 1000.0F, .max_speed = 1000.0F, .turn_responsiveness = 10.0F}
  );
  account.event_bus().process();

  const hyperverse::HomingMissile& ignited = world.registry.get<hyperverse::HomingMissile>(missile_entity);
  CHECK(ignited.phase == hyperverse::HomingMissilePhase::Ignited);
  CHECK(ignition_events == 1);
  CHECK(ignited.velocity.y > 0.0F);
}

TEST_CASE("homing missile damages locked raider on impact") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  const entt::entity player = make_player(world, {.x = 1000.0F, .y = 1000.0F});
  const entt::entity raider = world.registry.create();
  world.registry.emplace<hyperverse::RaiderShip>(
    raider,
    hyperverse::RaiderShip{.position = {.x = 120.0F, .y = 100.0F}, .integrity = 60.0F, .max_integrity = 60.0F}
  );
  const entt::entity missile_entity = world.registry.create();
  world.registry.emplace<hyperverse::HomingMissile>(
    missile_entity,
    hyperverse::HomingMissile{
      .position = {.x = 120.0F, .y = 100.0F},
      .target = raider,
      .damage = 35.0F,
      .phase = hyperverse::HomingMissilePhase::Ignited,
    }
  );
  int impact_events = 0;
  account.event_bus().appendListener(hyperverse::DomainEventType::HomingMissileImpact, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.subject == missile_entity);
    CHECK(event.target == raider);
    ++impact_events;
  });

  const hyperverse::HomingMissileHudSnapshot hud = hyperverse::update_homing_missiles(
    hyperverse::ProjectileSimCtx{tick_context(account, 0.0F), player}
  );
  account.event_bus().process();

  CHECK(hud.active_missiles == 0);
  CHECK(account.registry().get<hyperverse::RaiderShip>(raider).integrity == Catch::Approx(25.0F));
  CHECK(missile_count(account.registry()) == 0);
  CHECK(explosion_count(account.registry()) == 1);
  CHECK(impact_events == 1);

  hyperverse::update_explosion_bursts(account.registry(), 0.46F);
  CHECK(explosion_count(account.registry()) == 0);
}
