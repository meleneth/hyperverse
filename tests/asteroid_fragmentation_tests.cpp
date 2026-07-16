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
    CHECK(registry.get<hyperverse::AsteroidMass>(fragment).remaining_mass == Catch::Approx(40.0F));
    CHECK(registry.get<hyperverse::MiningResource>(fragment).tier == hyperverse::OreTier::Rare);
    CHECK(registry.all_of<hyperverse::MineralComposition>(fragment));
  }
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
