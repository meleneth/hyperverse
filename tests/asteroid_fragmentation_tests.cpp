#include "test_common.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_geometry.hpp"

#include <algorithm>
#include <array>
#include <map>

namespace {

[[nodiscard]] entt::entity make_fragmentable_asteroid(entt::registry& registry, hyperverse::Vec2 velocity = {}) {
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(asteroid, hyperverse::AsteroidBody{
                                                           .position = {.x = 100.0F, .y = 100.0F},
                                                           .velocity = velocity,
                                                           .radius = 240.0F,
                                                           .base_radius = 240.0F,
                                                           .scan_confidence = 0.5F,
                                                       });
  registry.emplace<hyperverse::AsteroidFragmentation>(asteroid,
                                                      hyperverse::AsteroidFragmentation{.remaining_breaks = 2});
  registry.emplace<hyperverse::AsteroidMass>(
      asteroid, hyperverse::AsteroidMass{.initial_mass = 240.0F, .remaining_mass = 160.0F});
  registry.emplace<hyperverse::MiningResource>(asteroid, hyperverse::MiningResource{.tier = hyperverse::OreTier::Rare});
  registry.emplace<hyperverse::MineralComposition>(asteroid,
                                                   hyperverse::mineral_composition_for_tier(hyperverse::OreTier::Rare));
  return asteroid;
}

[[nodiscard]] bool contains_parent_surface_color(const hyperverse::AsteroidGeometry& parent,
                                                 const hyperverse::AsteroidGeometry& child,
                                                 hyperverse::OreTint inherited_tint) {
  for (const hyperverse::AsteroidMeshVertex& child_vertex : child.vertices) {
    if (child_vertex.tint_blend != Catch::Approx(0.0F)) {
      continue;
    }
    for (const hyperverse::AsteroidMeshVertex& parent_vertex : parent.vertices) {
      if (child_vertex.r == Catch::Approx(parent_vertex.r * inherited_tint.r) &&
          child_vertex.g == Catch::Approx(parent_vertex.g * inherited_tint.g) &&
          child_vertex.b == Catch::Approx(parent_vertex.b * inherited_tint.b)) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool has_new_fracture_material(const hyperverse::AsteroidGeometry& child) {
  return std::ranges::any_of(child.vertices, [](const hyperverse::AsteroidMeshVertex& vertex) {
    return vertex.surface == hyperverse::AsteroidSurfaceKind::Fracture && vertex.tint_blend == Catch::Approx(0.0F);
  });
}

[[nodiscard]] float composition_total(const hyperverse::MineralComposition& composition) {
  return composition.silicate + composition.ferrite + composition.nickel + composition.cobalt + composition.iridium +
         composition.exotic_crystal + composition.anomalous_matter;
}

[[nodiscard]] int material_count(const hyperverse::MineralComposition& composition) {
  return static_cast<int>(composition.silicate > 0.001F) + static_cast<int>(composition.ferrite > 0.001F) +
         static_cast<int>(composition.nickel > 0.001F) + static_cast<int>(composition.cobalt > 0.001F) +
         static_cast<int>(composition.iridium > 0.001F) + static_cast<int>(composition.exotic_crystal > 0.001F) +
         static_cast<int>(composition.anomalous_matter > 0.001F);
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

  return static_cast<int>(
      std::ranges::count_if(edge_counts, [](const auto& edge_count) { return edge_count.second == 1; }));
}

} // namespace

TEST_CASE("laser fragmentation keeps child vectors nearly coherent") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry, {.x = 10.0F, .y = 5.0F});

  const std::vector<entt::entity> fragments =
      hyperverse::fragment_asteroid(registry, asteroid,
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Laser,
                                     .impact_velocity = {.x = 1000.0F, .y = 0.0F},
                                     .pieces = 4});

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

TEST_CASE("asteroid fragmentation allocates mixed child compositions from conserved material") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry);
  registry.replace<hyperverse::MineralComposition>(
      asteroid,
      hyperverse::MineralComposition{.silicate = 0.25F, .ferrite = 0.25F, .cobalt = 0.25F, .exotic_crystal = 0.25F});

  const std::vector<entt::entity> fragments = hyperverse::fragment_asteroid(
      registry, asteroid,
      {.impact_kind = hyperverse::AsteroidImpactKind::Laser, .impact_velocity = {.x = 100.0F, .y = 0.0F}, .pieces = 4});

  REQUIRE(fragments.size() == 3U);
  float recoverable_mass = 0.0F;
  bool found_common = false;
  bool found_industrial = false;
  bool found_rare = false;
  bool found_exotic = false;
  hyperverse::MineralComposition recovered{};
  std::array<float, hyperverse::OreTierCount> recovered_by_value_tier{};
  for (entt::entity fragment : fragments) {
    hyperverse::MiningResource& resource = registry.get<hyperverse::MiningResource>(fragment);
    const hyperverse::MineralComposition& composition = registry.get<hyperverse::MineralComposition>(fragment);
    recoverable_mass += registry.get<hyperverse::AsteroidMass>(fragment).remaining_mass;
    found_common = found_common || resource.tier == hyperverse::OreTier::Common;
    found_industrial = found_industrial || resource.tier == hyperverse::OreTier::Industrial;
    found_rare = found_rare || resource.tier == hyperverse::OreTier::Rare;
    found_exotic = found_exotic || resource.tier == hyperverse::OreTier::Exotic;
    const float mass = registry.get<hyperverse::AsteroidMass>(fragment).remaining_mass;
    recovered.silicate += mass * composition.silicate;
    recovered.ferrite += mass * composition.ferrite;
    recovered.cobalt += mass * composition.cobalt;
    recovered.exotic_crystal += mass * composition.exotic_crystal;
    hyperverse::record_extracted_material(resource, &composition, mass);
    for (std::size_t tier = 0; tier < recovered_by_value_tier.size(); ++tier) {
      recovered_by_value_tier[tier] += resource.extracted_mass_by_tier[tier];
    }
    CHECK(composition_total(composition) == Catch::Approx(1.0F));
    CHECK(material_count(composition) == 4);
  }

  CHECK(recoverable_mass == Catch::Approx(120.0F));
  CHECK(recovered.silicate == Catch::Approx(30.0F));
  CHECK(recovered.ferrite == Catch::Approx(30.0F));
  CHECK(recovered.cobalt == Catch::Approx(30.0F));
  CHECK(recovered.exotic_crystal == Catch::Approx(30.0F));
  CHECK(recovered_by_value_tier[0] == Catch::Approx(30.0F));
  CHECK(recovered_by_value_tier[1] == Catch::Approx(30.0F));
  CHECK(recovered_by_value_tier[2] == Catch::Approx(30.0F));
  CHECK(recovered_by_value_tier[3] == Catch::Approx(30.0F));
  CHECK(recovered_by_value_tier[4] == Catch::Approx(0.0F));
  const bool found_any_expected_tier = found_common || found_industrial || found_rare || found_exotic;
  const bool recovered_every_component = found_common && found_industrial && found_rare && found_exotic;
  CHECK(found_any_expected_tier);
  CHECK_FALSE(recovered_every_component);
}

TEST_CASE("fragment material allocation and exposed colors are deterministic across repeated breaks") {
  entt::registry first_registry;
  entt::registry second_registry;
  const entt::entity first_root = make_fragmentable_asteroid(first_registry);
  const entt::entity second_root = make_fragmentable_asteroid(second_registry);
  const hyperverse::MineralComposition composition{
      .silicate = 0.18F,
      .ferrite = 0.22F,
      .cobalt = 0.25F,
      .exotic_crystal = 0.35F,
  };
  first_registry.replace<hyperverse::MineralComposition>(first_root, composition);
  second_registry.replace<hyperverse::MineralComposition>(second_root, composition);
  first_registry.emplace<hyperverse::AsteroidGeometry>(first_root,
                                                       hyperverse::generate_asteroid_geometry(9001U, 240.0F));
  second_registry.emplace<hyperverse::AsteroidGeometry>(second_root,
                                                        hyperverse::generate_asteroid_geometry(9001U, 240.0F));
  const hyperverse::AsteroidFragmentationRequest request{
      .impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
      .impact_position = {.x = 80.0F, .y = 100.0F},
      .impact_velocity = {.x = 500.0F, .y = 20.0F},
      .pieces = 4,
  };

  std::vector<entt::entity> first = hyperverse::fragment_asteroid(first_registry, first_root, request);
  std::vector<entt::entity> second = hyperverse::fragment_asteroid(second_registry, second_root, request);
  REQUIRE(first.size() == second.size());
  for (std::size_t index = 0; index < first.size(); ++index) {
    const auto& first_mass = first_registry.get<hyperverse::AsteroidMass>(first[index]);
    const auto& second_mass = second_registry.get<hyperverse::AsteroidMass>(second[index]);
    const auto& first_composition = first_registry.get<hyperverse::MineralComposition>(first[index]);
    const auto& second_composition = second_registry.get<hyperverse::MineralComposition>(second[index]);
    CHECK(first_mass.remaining_mass == Catch::Approx(second_mass.remaining_mass));
    CHECK(first_composition.silicate == Catch::Approx(second_composition.silicate));
    CHECK(first_composition.ferrite == Catch::Approx(second_composition.ferrite));
    CHECK(first_composition.cobalt == Catch::Approx(second_composition.cobalt));
    CHECK(first_composition.exotic_crystal == Catch::Approx(second_composition.exotic_crystal));
    const auto& first_geometry = first_registry.get<hyperverse::AsteroidGeometry>(first[index]);
    const auto& second_geometry = second_registry.get<hyperverse::AsteroidGeometry>(second[index]);
    REQUIRE(first_geometry.vertices.size() == second_geometry.vertices.size());
    for (std::size_t vertex = 0; vertex < first_geometry.vertices.size(); ++vertex) {
      CHECK(first_geometry.vertices[vertex].r == Catch::Approx(second_geometry.vertices[vertex].r));
      CHECK(first_geometry.vertices[vertex].g == Catch::Approx(second_geometry.vertices[vertex].g));
      CHECK(first_geometry.vertices[vertex].b == Catch::Approx(second_geometry.vertices[vertex].b));
      CHECK(first_geometry.vertices[vertex].surface == second_geometry.vertices[vertex].surface);
    }
  }

  const hyperverse::AsteroidGeometry& exposed_parent_geometry =
      first_registry.get<hyperverse::AsteroidGeometry>(first.front());
  const auto exposed = std::ranges::find_if(exposed_parent_geometry.vertices, [](const auto& vertex) {
    return vertex.surface == hyperverse::AsteroidSurfaceKind::Fracture;
  });
  REQUIRE(exposed != exposed_parent_geometry.vertices.end());
  const hyperverse::AsteroidMeshVertex exposed_color = *exposed;

  first = hyperverse::fragment_asteroid(first_registry, first.front(), request);
  second = hyperverse::fragment_asteroid(second_registry, second.front(), request);
  REQUIRE(first.size() == second.size());
  REQUIRE_FALSE(first.empty());
  CHECK(first_registry.get<hyperverse::AsteroidMass>(first.front()).remaining_mass ==
        Catch::Approx(second_registry.get<hyperverse::AsteroidMass>(second.front()).remaining_mass));
  CHECK(std::ranges::any_of(first, [&](entt::entity fragment) {
    const auto& geometry = first_registry.get<hyperverse::AsteroidGeometry>(fragment);
    return std::ranges::any_of(geometry.vertices, [&](const auto& vertex) {
      return vertex.surface == hyperverse::AsteroidSurfaceKind::Fracture &&
             vertex.r == Catch::Approx(exposed_color.r) && vertex.g == Catch::Approx(exposed_color.g) &&
             vertex.b == Catch::Approx(exposed_color.b);
    });
  }));
}

TEST_CASE("asteroids only break into multiples for two levels") {
  entt::registry registry;
  const entt::entity root = make_fragmentable_asteroid(registry);
  hyperverse::AsteroidBody& root_body = registry.get<hyperverse::AsteroidBody>(root);
  root_body.radius = 600.0F;
  root_body.base_radius = 600.0F;

  const std::vector<entt::entity> first_level =
      hyperverse::fragment_asteroid(registry, root,
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                     .impact_velocity = {.x = 100.0F, .y = 0.0F},
                                     .pieces = 4});

  REQUIRE(first_level.size() == 3U);
  const std::vector<entt::entity> second_level =
      hyperverse::fragment_asteroid(registry, first_level.front(),
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                     .impact_velocity = {.x = 100.0F, .y = 0.0F},
                                     .pieces = 4});

  REQUIRE(second_level.size() == 3U);
  CHECK(registry.get<hyperverse::AsteroidFragmentation>(second_level.front()).remaining_breaks == 0);

  const std::vector<entt::entity> terminal =
      hyperverse::fragment_asteroid(registry, second_level.front(),
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                     .impact_velocity = {.x = 100.0F, .y = 0.0F},
                                     .pieces = 4});

  CHECK(terminal.empty());
  CHECK_FALSE(registry.valid(second_level.front()));
}

TEST_CASE("asteroid fragmentation emits lifecycle events") {
  entt::registry registry;
  hyperverse::DomainEventBus event_bus;
  int fragmented_events = 0;
  int consumed_events = 0;
  event_bus.appendListener(hyperverse::DomainEventType::AsteroidFragmented, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.type == hyperverse::DomainEventType::AsteroidFragmented);
    CHECK(event.count == 3);
    CHECK(event.amount == Catch::Approx(0.0F));
    ++fragmented_events;
  });
  event_bus.appendListener(hyperverse::DomainEventType::AsteroidConsumed, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.type == hyperverse::DomainEventType::AsteroidConsumed);
    ++consumed_events;
  });

  const entt::entity splitting = make_fragmentable_asteroid(registry);
  registry.get<hyperverse::AsteroidFragmentation>(splitting).remaining_breaks = 1;
  const std::vector<entt::entity> fragments =
      hyperverse::fragment_asteroid(registry, event_bus, splitting,
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                     .impact_velocity = {.x = 100.0F, .y = 0.0F},
                                     .pieces = 4});

  REQUIRE(fragments.size() == 3U);
  event_bus.process();
  CHECK(fragmented_events == 1);
  CHECK(consumed_events == 0);

  (void)hyperverse::fragment_asteroid(registry, event_bus, fragments.front(),
                                      {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                       .impact_velocity = {.x = 100.0F, .y = 0.0F},
                                       .pieces = 4});
  event_bus.process();

  CHECK(fragmented_events == 1);
  CHECK(consumed_events == 1);
}

TEST_CASE("kinetic fragmentation transfers projectile velocity into every child") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry);

  const std::vector<entt::entity> fragments =
      hyperverse::fragment_asteroid(registry, asteroid,
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                     .impact_velocity = {.x = 900.0F, .y = 0.0F},
                                     .pieces = 4});

  REQUIRE(fragments.size() == 3U);
  for (entt::entity fragment : fragments) {
    CHECK(registry.get<hyperverse::AsteroidBody>(fragment).velocity.x == Catch::Approx(252.0F));
  }
}

TEST_CASE("explosive fragmentation scatters children in opposing directions") {
  entt::registry registry;
  const entt::entity asteroid = make_fragmentable_asteroid(registry);

  const std::vector<entt::entity> fragments =
      hyperverse::fragment_asteroid(registry, asteroid,
                                    {.impact_kind = hyperverse::AsteroidImpactKind::Explosive,
                                     .impact_velocity = {.x = 900.0F, .y = 0.0F},
                                     .pieces = 4});

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

  const std::vector<entt::entity> fragments =
      hyperverse::fragment_asteroid(registry, asteroid,
                                    {
                                        .impact_kind = hyperverse::AsteroidImpactKind::Kinetic,
                                        .impact_position = {.x = 90.0F, .y = 100.0F},
                                        .impact_velocity = {.x = 900.0F, .y = 0.0F},
                                        .pieces = 4,
                                    });

  REQUIRE(fragments.size() == 3U);
  const hyperverse::OreTint inherited_tint = hyperverse::ore_tint(hyperverse::OreTier::Rare);
  for (entt::entity fragment : fragments) {
    REQUIRE(registry.all_of<hyperverse::AsteroidGeometry>(fragment));
    const hyperverse::AsteroidGeometry& geometry = registry.get<hyperverse::AsteroidGeometry>(fragment);
    const hyperverse::OreTint fracture_tint =
        hyperverse::ore_tint(registry.get<hyperverse::MineralComposition>(fragment));
    CHECK_FALSE(geometry.vertices.empty());
    CHECK_FALSE(geometry.triangles.empty());
    CHECK(contains_parent_surface_color(parent_geometry, geometry, inherited_tint));
    CHECK(has_new_fracture_material(geometry));
    for (const hyperverse::AsteroidMeshVertex& vertex : geometry.vertices) {
      if (vertex.surface != hyperverse::AsteroidSurfaceKind::Fracture)
        continue;
      CHECK((vertex.r / vertex.g) == Catch::Approx(fracture_tint.r / fracture_tint.g));
      CHECK((vertex.b / vertex.g) == Catch::Approx(fracture_tint.b / fracture_tint.g));
    }
    CHECK(geometry.tumble_velocity.z > 0.0F);
    CHECK(open_edge_count(geometry) == 0);
  }
}
