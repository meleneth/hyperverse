#include "test_common.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_geometry.hpp"

#include <algorithm>
#include <map>

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

[[nodiscard]] bool contains_parent_surface_color(const hyperverse::AsteroidGeometry& parent, const hyperverse::AsteroidGeometry& child, hyperverse::OreTint inherited_tint) {
  for (const hyperverse::AsteroidMeshVertex& child_vertex : child.vertices) {
    if (child_vertex.tint_blend != Catch::Approx(0.0F)) {
      continue;
    }
    for (const hyperverse::AsteroidMeshVertex& parent_vertex : parent.vertices) {
      if (
        child_vertex.r == Catch::Approx(parent_vertex.r * inherited_tint.r) &&
        child_vertex.g == Catch::Approx(parent_vertex.g * inherited_tint.g) &&
        child_vertex.b == Catch::Approx(parent_vertex.b * inherited_tint.b)
      ) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool has_new_fracture_material(const hyperverse::AsteroidGeometry& child) {
  return std::ranges::any_of(child.vertices, [](const hyperverse::AsteroidMeshVertex& vertex) {
    return vertex.tint_blend == Catch::Approx(1.0F);
  });
}

struct EdgeKey {
  std::uint16_t a{};
  std::uint16_t b{};

  [[nodiscard]] auto operator<=>(const EdgeKey&) const = default;
};

[[nodiscard]] EdgeKey edge_key(std::uint16_t lhs, std::uint16_t rhs) {
  return lhs < rhs ? EdgeKey{.a = lhs, .b = rhs} : EdgeKey{.a = rhs, .b = lhs};
}

[[nodiscard]] int open_edge_count(const hyperverse::AsteroidGeometry& geometry) {
  std::map<EdgeKey, int> edge_counts;
  for (const hyperverse::AsteroidMeshTriangle& triangle : geometry.triangles) {
    ++edge_counts[edge_key(triangle.a, triangle.b)];
    ++edge_counts[edge_key(triangle.b, triangle.c)];
    ++edge_counts[edge_key(triangle.c, triangle.a)];
  }

  return static_cast<int>(std::ranges::count_if(edge_counts, [](const auto& edge_count) {
    return edge_count.second == 1;
  }));
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

  REQUIRE(fragments.size() == 3U);
  CHECK_FALSE(registry.valid(asteroid));
  for (entt::entity fragment : fragments) {
    const hyperverse::AsteroidBody& body = registry.get<hyperverse::AsteroidBody>(fragment);
    CHECK(body.radius == Catch::Approx(240.0F / std::sqrt(3.0F)));
    CHECK(body.velocity.x == Catch::Approx(50.0F));
    CHECK(body.velocity.y > -5.0F);
    CHECK(body.velocity.y < 20.0F);
    CHECK(registry.get<hyperverse::AsteroidFragmentation>(fragment).remaining_breaks == 1);
    CHECK(registry.all_of<hyperverse::MineralComposition>(fragment));
  }
}

TEST_CASE("asteroid fragmentation separates recoverable component chunks") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry);
  registry.replace<hyperverse::MineralComposition>(
    asteroid,
    hyperverse::MineralComposition{.silicate = 0.25F, .ferrite = 0.25F, .cobalt = 0.25F, .exotic_crystal = 0.25F}
  );

  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
    registry,
    asteroid,
    {.impact_kind = hyperverse::AsteroidImpactKind::Laser, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4}
  );

  REQUIRE(fragments.size() == 3U);
  float recoverable_mass = 0.0F;
  bool found_common = false;
  bool found_industrial = false;
  bool found_rare = false;
  bool found_exotic = false;
  for (entt::entity fragment : fragments) {
    const hyperverse::MiningResource& resource = registry.get<hyperverse::MiningResource>(fragment);
    const hyperverse::MineralComposition& composition = registry.get<hyperverse::MineralComposition>(fragment);
    recoverable_mass += registry.get<hyperverse::AsteroidMass>(fragment).remaining_mass;
    found_common = found_common || resource.tier == hyperverse::OreTier::Common;
    found_industrial = found_industrial || resource.tier == hyperverse::OreTier::Industrial;
    found_rare = found_rare || resource.tier == hyperverse::OreTier::Rare;
    found_exotic = found_exotic || resource.tier == hyperverse::OreTier::Exotic;
    const bool is_component_chunk =
      composition.silicate == Catch::Approx(1.0F) ||
      composition.ferrite == Catch::Approx(1.0F) ||
      composition.cobalt == Catch::Approx(1.0F) ||
      composition.exotic_crystal == Catch::Approx(1.0F);
    CHECK(is_component_chunk);
  }

  CHECK(recoverable_mass == Catch::Approx(120.0F));
  const bool found_any_expected_tier = found_common || found_industrial || found_rare || found_exotic;
  const bool recovered_every_component = found_common && found_industrial && found_rare && found_exotic;
  CHECK(found_any_expected_tier);
  CHECK_FALSE(recovered_every_component);
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

  REQUIRE(first_level.size() == 3U);
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
      CHECK(event.count == 3);
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

  REQUIRE(fragments.size() == 3U);
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

  REQUIRE(fragments.size() == 3U);
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

  REQUIRE(fragments.size() == 3U);
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.x > 100.0F;
  }));
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.x < -60.0F;
  }));
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.y > 100.0F;
  }));
  CHECK(std::ranges::any_of(fragments, [&](entt::entity fragment) {
    return registry.get<hyperverse::AsteroidBody>(fragment).velocity.y < -100.0F;
  }));
}

TEST_CASE("asteroid fragmentation splits generated geometry into renderable chunks") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry, {.x = 10.0F, .y = 5.0F});
  hyperverse::AsteroidGeometry parent_geometry = hyperverse::generate_asteroid_geometry(48879U, 240.0F);
  parent_geometry.tumble_velocity = {.x = 0.14F, .y = 0.08F, .z = 0.22F};
  registry.emplace<hyperverse::AsteroidGeometry>(asteroid, parent_geometry);

  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
    registry,
    asteroid,
    {
      .impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
      .impact_position = {.x = 90.0F, .y = 100.0F},
      .impact_velocity = {.x = 900.0F, .y = 0.0F},
      .pieces = 4,
    }
  );

  REQUIRE(fragments.size() == 3U);
  const hyperverse::OreTint inherited_tint = hyperverse::ore_tint(hyperverse::OreTier::Rare);
  for (entt::entity fragment : fragments) {
    REQUIRE(registry.all_of<hyperverse::AsteroidGeometry>(fragment));
    const hyperverse::AsteroidGeometry& geometry = registry.get<hyperverse::AsteroidGeometry>(fragment);
    CHECK_FALSE(geometry.vertices.empty());
    CHECK_FALSE(geometry.triangles.empty());
    CHECK(contains_parent_surface_color(parent_geometry, geometry, inherited_tint));
    CHECK(has_new_fracture_material(geometry));
    CHECK(geometry.tumble_velocity.z > 0.0F);
    CHECK(open_edge_count(geometry) == 0);
  }
}
