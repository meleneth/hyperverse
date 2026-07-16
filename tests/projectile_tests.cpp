#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("particle cannon spawns a shot from semantic fire") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  int fired_events = 0;
  account.event_bus().appendListener(
    hyperverse::DomainEventType::ParticleFired,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::ParticleFired);
      ++fired_events;
    }
  );

  const hyperverse::ParticleCannonHudSnapshot hud = hyperverse::update_particle_cannon(
    account,
    {.position = {.x = 100.0F, .y = 100.0F}, .facing_radians = 0.0F},
    {.particle_fire_requested = true},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    {.projectile_speed = 100.0F}
  );
  account.event_bus().process();

  CHECK(hud.active_particles == 1);
  CHECK(fired_events == 1);
  CHECK(std::distance(account.registry().view<hyperverse::ParticleShot>().begin(), account.registry().view<hyperverse::ParticleShot>().end()) == 1);
}

TEST_CASE("particle cannon damages and shrinks asteroids on Jolt overlap") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  hyperverse::ParticleCannonHudSnapshot event_hud{};
  account.event_bus().appendListener(
    hyperverse::DomainEventType::ParticleImpact,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::ParticleImpact);
      ++event_hud.impacts;
    }
  );
  const entt::entity asteroid = account.registry().create();
  account.registry().emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}, .radius = 80.0F, .base_radius = 80.0F}
  );
  account.registry().emplace<hyperverse::MiningResource>(asteroid);
  const entt::entity particle = account.registry().create();
  account.registry().emplace<hyperverse::ParticleShot>(particle, hyperverse::ParticleShot{.position = {.x = 110.0F, .y = 100.0F}});

  const hyperverse::ParticleCannonHudSnapshot hud = hyperverse::update_particle_cannon(
    account,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    {.projectile_radius = 10.0F, .damage = 25.0F}
  );
  account.event_bus().process();

  CHECK(hud.impacts == 0);
  CHECK(event_hud.impacts == 1);
  CHECK(account.registry().get<hyperverse::MiningResource>(asteroid).integrity == Catch::Approx(75.0F));
  CHECK(account.registry().get<hyperverse::AsteroidBody>(asteroid).radius == Catch::Approx(60.0F));
  CHECK(std::distance(account.registry().view<hyperverse::ParticleShot>().begin(), account.registry().view<hyperverse::ParticleShot>().end()) == 0);
}
