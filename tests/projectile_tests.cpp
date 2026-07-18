#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

namespace {

[[nodiscard]] std::ptrdiff_t particle_count(entt::registry& registry) {
  return std::distance(registry.view<hyperverse::ParticleShot>().begin(), registry.view<hyperverse::ParticleShot>().end());
}

[[nodiscard]] entt::entity make_player(TestAccountWorld& world, hyperverse::Vec2 position) {
  const entt::entity player = world.registry.create();
  world.registry.emplace<hyperverse::ShipMotion>(player, hyperverse::ShipMotion{.position = position});
  world.registry.emplace<hyperverse::ShipHealth>(player);
  world.registry.emplace<hyperverse::ParticleCannonModel>(player);
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
