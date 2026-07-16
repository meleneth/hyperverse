#include "test_common.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"

#include <algorithm>

namespace {

[[nodiscard]] entt::entity make_fragmentable_asteroid(entt::registry& registry, hyperverse::Vec2 velocity = {}) {
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{
      .position = {.x = 100.0F, .y = 100.0F},
      .velocity = velocity,
      .radius = 240.0F,
      .base_radius = 240.0F,
      .scan_confidence = 0.5F,
    }
  );
  registry.emplace<hyperverse::AsteroidFragmentation>(asteroid, hyperverse::AsteroidFragmentation{.remaining_breaks = 2});
  registry.emplace<hyperverse::AsteroidMass>(asteroid, hyperverse::AsteroidMass{.initial_mass = 240.0F, .remaining_mass = 160.0F});
  registry.emplace<hyperverse::MiningResource>(asteroid, hyperverse::MiningResource{.tier = hyperverse::OreTier::Rare});
  registry.emplace<hyperverse::MineralComposition>(asteroid, hyperverse::mineral_composition_for_tier(hyperverse::OreTier::Rare));
  return asteroid;
}

}  // namespace

TEST_CASE("laser fragmentation keeps child vectors nearly coherent") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry, {.x = 10.0F, .y = 5.0F});

  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
    registry,
    asteroid,
    {.impact_kind = hyperverse::AsteroidImpactKind::Laser, .impact_velocity = {.x = 1000.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(fragments.size() == 4U);
  CHECK_FALSE(registry.valid(asteroid));
  for (entt::entity fragment : fragments) {
    const hyperverse::AsteroidBody& body = registry.get<hyperverse::AsteroidBody>(fragment);
    CHECK(body.radius == Catch::Approx(120.0F));
    CHECK(body.velocity.x == Catch::Approx(50.0F));
    CHECK(body.velocity.y > -5.0F);
    CHECK(body.velocity.y < 20.0F);
    CHECK(registry.get<hyperverse::AsteroidFragmentation>(fragment).remaining_breaks == 1);
    CHECK(registry.get<hyperverse::AsteroidMass>(fragment).remaining_mass == Catch::Approx(40.0F));
    CHECK(registry.get<hyperverse::MiningResource>(fragment).tier == hyperverse::OreTier::Rare);
    CHECK(registry.all_of<hyperverse::MineralComposition>(fragment));
  }
}

TEST_CASE("asteroids only break into multiples for two levels") {
  entt::registry registry;
  const entt::entity root = make_fragmentable_asteroid(registry);
  hyperverse::AsteroidBody& root_body = registry.get<hyperverse::AsteroidBody>(root);
  root_body.radius = 600.0F;
  root_body.base_radius = 600.0F;

  const std::vector<entt::entity> first_level = hyperverse::fragment_asteroid(
    registry,
    root,
    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(first_level.size() == 4U);
  const std::vector<entt::entity> second_level = hyperverse::fragment_asteroid(
    registry,
    first_level.front(),
    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(second_level.size() == 4U);
  CHECK(registry.get<hyperverse::AsteroidFragmentation>(second_level.front()).remaining_breaks == 0);

  const std::vector<entt::entity> terminal = hyperverse::fragment_asteroid(
    registry,
    second_level.front(),
    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4}
  );

  CHECK(terminal.empty());
  CHECK_FALSE(registry.valid(second_level.front()));
}

TEST_CASE("asteroid fragmentation emits lifecycle events") {
  entt::registry registry;
  hyperverse::DomainEventBus event_bus;
  int fragmented_events = 0;
  int consumed_events = 0;
  event_bus.appendListener(
    hyperverse::DomainEventType::AsteroidFragmented,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::AsteroidFragmented);
      CHECK(event.count == 4);
      CHECK(event.amount == Catch::Approx(0.0F));
      ++fragmented_events;
    }
  );
  event_bus.appendListener(
    hyperverse::DomainEventType::AsteroidConsumed,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::AsteroidConsumed);
      ++consumed_events;
    }
  );

  const entt::entity splitting = make_fragmentable_asteroid(registry);
  registry.get<hyperverse::AsteroidFragmentation>(splitting).remaining_breaks = 1;
  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
    registry,
    event_bus,
    splitting,
    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(fragments.size() == 4U);
  event_bus.process();
  CHECK(fragmented_events == 1);
  CHECK(consumed_events == 0);

  (void)hyperverse::fragment_asteroid(
    registry,
    event_bus,
    fragments.front(),
    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4}
  );
  event_bus.process();

  CHECK(fragmented_events == 1);
  CHECK(consumed_events == 1);
}

TEST_CASE("kinetic fragmentation transfers projectile velocity into every child") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry);

  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
    registry,
    asteroid,
    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic, .impact_velocity = {.x = 900.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(fragments.size() == 4U);
  for (entt::entity fragment : fragments) {
    CHECK(registry.get<hyperverse::AsteroidBody>(fragment).velocity.x == Catch::Approx(252.0F));
  }
}

TEST_CASE("explosive fragmentation scatters children in opposing directions") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry);

  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
    registry,
    asteroid,
    {.impact_kind = hyperverse::AsteroidImpactKind::Explosive, .impact_velocity = {.x = 900.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(fragments.size() == 4U);
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.x > 100.0F;
  }));
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.x < -100.0F;
  }));
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.y > 100.0F;
  }));
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.y < -100.0F;
  }));
}
