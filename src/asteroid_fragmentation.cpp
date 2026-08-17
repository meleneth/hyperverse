#include "hyperverse/asteroid_fragmentation.hpp"

#include "hyperverse/asteroid_geometry.hpp"
#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/mining.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <utility>

namespace hyperverse {
namespace {

[[nodiscard]] Vec2 perpendicular(Vec2 value) {
  return {.x = -value.y, .y = value.x};
}

[[nodiscard]] Vec2 direction_from_angle(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

enum class MineralKind {
  Silicate,
  Ferrite,
  Nickel,
  Cobalt,
  Iridium,
  ExoticCrystal,
  AnomalousMatter,
};

struct MineralShare {
  MineralKind kind{MineralKind::Silicate};
  float amount{0.0F};
};

struct FragmentMaterialAllocation {
  MineralComposition composition{};
  float mass_fraction{0.0F};
  OreTier tier{OreTier::Common};
};

void add_mineral(MineralComposition& composition, MineralKind kind, float amount) {
  switch (kind) {
  case MineralKind::Silicate:
    composition.silicate += amount;
    break;
  case MineralKind::Ferrite:
    composition.ferrite += amount;
    break;
  case MineralKind::Nickel:
    composition.nickel += amount;
    break;
  case MineralKind::Cobalt:
    composition.cobalt += amount;
    break;
  case MineralKind::Iridium:
    composition.iridium += amount;
    break;
  case MineralKind::ExoticCrystal:
    composition.exotic_crystal += amount;
    break;
  case MineralKind::AnomalousMatter:
    composition.anomalous_matter += amount;
    break;
  }
}

[[nodiscard]] OreTier tier_for_share(MineralKind kind) {
  switch (kind) {
  case MineralKind::Silicate:
    return OreTier::Common;
  case MineralKind::Ferrite:
  case MineralKind::Nickel:
    return OreTier::Industrial;
  case MineralKind::Cobalt:
  case MineralKind::Iridium:
    return OreTier::Rare;
  case MineralKind::ExoticCrystal:
    return OreTier::Exotic;
  case MineralKind::AnomalousMatter:
    return OreTier::Anomalous;
  }

  return OreTier::Common;
}

[[nodiscard]] std::vector<MineralShare> mineral_shares(const MineralComposition& composition) {
  std::vector<MineralShare> shares{
      {.kind = MineralKind::Silicate, .amount = composition.silicate},
      {.kind = MineralKind::Ferrite, .amount = composition.ferrite},
      {.kind = MineralKind::Nickel, .amount = composition.nickel},
      {.kind = MineralKind::Cobalt, .amount = composition.cobalt},
      {.kind = MineralKind::Iridium, .amount = composition.iridium},
      {.kind = MineralKind::ExoticCrystal, .amount = composition.exotic_crystal},
      {.kind = MineralKind::AnomalousMatter, .amount = composition.anomalous_matter},
  };
  std::erase_if(shares, [](const MineralShare& share) { return share.amount <= 0.001F; });
  std::ranges::sort(shares, [](const MineralShare& lhs, const MineralShare& rhs) { return lhs.amount > rhs.amount; });
  return shares;
}

[[nodiscard]] float allocation_weight(std::uint32_t seed, std::size_t mineral, int fragment) {
  std::uint32_t value = seed ^ (0x9E3779B9U * static_cast<std::uint32_t>(mineral + 1U));
  value ^= 0x85EBCA6BU * static_cast<std::uint32_t>(fragment + 1);
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  return 0.65F + static_cast<float>(value & 0xFFFFU) * (0.70F / 65535.0F);
}

[[nodiscard]] std::vector<FragmentMaterialAllocation> allocate_fragment_materials(const MineralComposition& composition,
                                                                                  int requested_pieces,
                                                                                  std::uint32_t seed,
                                                                                  bool preserve_requested_piece_count) {
  std::vector<MineralShare> minerals = mineral_shares(composition);
  if (minerals.empty()) {
    minerals.push_back({.kind = MineralKind::Silicate, .amount = 1.0F});
  }
  const float composition_total =
      std::accumulate(minerals.begin(), minerals.end(), 0.0F,
                      [](float total, const MineralShare& share) { return total + share.amount; });
  for (MineralShare& mineral : minerals)
    mineral.amount /= composition_total;

  int fragment_count = requested_pieces;
  if (!preserve_requested_piece_count && minerals.size() > 1U) {
    fragment_count = std::min(requested_pieces, std::max(2, static_cast<int>(minerals.size())));
  }
  if (!preserve_requested_piece_count && minerals.size() >= 4U && requested_pieces >= 4) {
    fragment_count = std::min(fragment_count, requested_pieces - 1);
  }
  fragment_count = std::clamp(fragment_count, 2, 6);
  const float recovery_fraction = static_cast<float>(fragment_count) / static_cast<float>(requested_pieces);

  std::vector<FragmentMaterialAllocation> allocations(static_cast<std::size_t>(fragment_count));
  for (std::size_t mineral_index = 0; mineral_index < minerals.size(); ++mineral_index) {
    float weight_total = 0.0F;
    for (int fragment = 0; fragment < fragment_count; ++fragment) {
      weight_total += allocation_weight(seed, mineral_index, fragment);
    }
    for (int fragment = 0; fragment < fragment_count; ++fragment) {
      const float mineral_fraction = minerals[mineral_index].amount * recovery_fraction *
                                     allocation_weight(seed, mineral_index, fragment) / weight_total;
      add_mineral(allocations[static_cast<std::size_t>(fragment)].composition, minerals[mineral_index].kind,
                  mineral_fraction);
      allocations[static_cast<std::size_t>(fragment)].mass_fraction += mineral_fraction;
    }
  }

  for (FragmentMaterialAllocation& allocation : allocations) {
    const float inverse_mass = 1.0F / std::max(allocation.mass_fraction, 0.0001F);
    allocation.composition.silicate *= inverse_mass;
    allocation.composition.ferrite *= inverse_mass;
    allocation.composition.nickel *= inverse_mass;
    allocation.composition.cobalt *= inverse_mass;
    allocation.composition.iridium *= inverse_mass;
    allocation.composition.exotic_crystal *= inverse_mass;
    allocation.composition.anomalous_matter *= inverse_mass;
    const std::vector<MineralShare> child_minerals = mineral_shares(allocation.composition);
    allocation.tier = tier_for_share(child_minerals.front().kind);
  }
  return allocations;
}

[[nodiscard]] Vec2 impact_direction(const AsteroidFragmentationRequest& request, const AsteroidBody& parent) {
  const Vec2 velocity_direction = normalize_or_zero(request.impact_velocity);
  if (length(velocity_direction) > 0.0F) {
    return velocity_direction;
  }

  const Vec2 position_direction = normalize_or_zero(parent.position - request.impact_position);
  if (length(position_direction) > 0.0F) {
    return position_direction;
  }

  return {.x = 1.0F, .y = 0.0F};
}

[[nodiscard]] Vec2 fragment_velocity(const AsteroidBody& parent, const AsteroidFragmentationRequest& request, int index,
                                     int pieces) {
  const Vec2 forward = impact_direction(request, parent);
  const Vec2 tangent = perpendicular(forward);
  const float impact_speed = length(request.impact_velocity);
  const float centered_index = static_cast<float>(index) - ((static_cast<float>(pieces) - 1.0F) * 0.5F);

  switch (request.impact_kind) {
  case AsteroidImpactKind::Laser:
    return parent.velocity + (forward * std::clamp(impact_speed * 0.04F, 8.0F, 70.0F)) +
           (tangent * centered_index * 4.0F);
  case AsteroidImpactKind::Kinetic:
    return parent.velocity + (request.impact_velocity * 0.28F) + (tangent * centered_index * 8.0F);
  case AsteroidImpactKind::Explosive: {
    const float angle = (static_cast<float>(index) / static_cast<float>(pieces)) * std::numbers::pi_v<float> * 2.0F;
    const float scatter_speed = std::clamp(impact_speed * 0.18F, 90.0F, 360.0F);
    return parent.velocity + (direction_from_angle(angle) * scatter_speed);
  }
  }

  return parent.velocity;
}

[[nodiscard]] int remaining_breaks_for(entt::registry& registry, entt::entity asteroid) {
  const AsteroidFragmentation* fragmentation = registry.try_get<AsteroidFragmentation>(asteroid);
  return fragmentation != nullptr ? fragmentation->remaining_breaks : 0;
}

void emit_consumed(DomainEventBus* event_bus, entt::entity asteroid, Vec2 position) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(DomainEventType::AsteroidConsumed, DomainEvent{
                                                            .type = DomainEventType::AsteroidConsumed,
                                                            .subject = asteroid,
                                                            .position = position,
                                                        });
}

void emit_fragmented(DomainEventBus* event_bus, entt::entity asteroid, Vec2 position, int fragment_count,
                     int child_remaining_breaks) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(DomainEventType::AsteroidFragmented, DomainEvent{
                                                              .type = DomainEventType::AsteroidFragmented,
                                                              .subject = asteroid,
                                                              .position = position,
                                                              .amount = static_cast<float>(child_remaining_breaks),
                                                              .count = fragment_count,
                                                          });
}

[[nodiscard]] std::vector<entt::entity> fragment_asteroid_impl(entt::registry& registry, DomainEventBus* event_bus,
                                                               entt::entity asteroid,
                                                               const AsteroidFragmentationRequest& request) {
  if (!registry.valid(asteroid) || !registry.all_of<AsteroidBody>(asteroid) || request.pieces < 2) {
    return {};
  }

  const AsteroidBody parent = registry.get<AsteroidBody>(asteroid);
  const AsteroidGeometry* parent_geometry = registry.try_get<AsteroidGeometry>(asteroid);
  const int parent_remaining_breaks = remaining_breaks_for(registry, asteroid);
  const MineralComposition* parent_composition = registry.try_get<MineralComposition>(asteroid);
  const MiningResource* parent_resource = registry.try_get<MiningResource>(asteroid);
  const MineralComposition fallback_composition = parent_resource != nullptr
                                                      ? mineral_composition_for_tier(parent_resource->tier)
                                                      : mineral_composition_for_tier(OreTier::Common);
  const MineralComposition& source_composition =
      parent_composition != nullptr ? *parent_composition : fallback_composition;
  const std::uint32_t allocation_seed =
      parent_geometry != nullptr ? parent_geometry->seed : static_cast<std::uint32_t>(entt::to_integral(asteroid));
  const std::vector<FragmentMaterialAllocation> allocations =
      allocate_fragment_materials(source_composition, request.pieces, allocation_seed, parent_composition == nullptr);
  const int fragment_count = static_cast<int>(allocations.size());
  const float child_radius = std::max(8.0F, parent.radius / std::sqrt(static_cast<float>(fragment_count)));
  if (parent_remaining_breaks <= 0 || child_radius < MinimumPlayableAsteroidRadius) {
    registry.destroy(asteroid);
    emit_consumed(event_bus, asteroid, parent.position);
    return {};
  }

  const AsteroidMass* parent_mass = registry.try_get<AsteroidMass>(asteroid);
  const int child_remaining_breaks = parent_remaining_breaks - 1;
  const float placement_radius = std::max(child_radius, parent.radius - child_radius);
  std::vector<AsteroidGeometry> child_geometries;
  if (parent_geometry != nullptr) {
    Vec2 fracture_direction = impact_direction(request, parent);
    if (length(request.impact_position) > 0.0F) {
      const Vec2 from_parent_to_impact = normalize_or_zero(request.impact_position - parent.position);
      if (length(from_parent_to_impact) > 0.0F) {
        fracture_direction = from_parent_to_impact;
      }
    }
    OreTint inherited_tint{.r = 0.82F, .g = 0.78F, .b = 0.70F};
    if (parent_resource != nullptr) {
      inherited_tint = ore_tint(parent_resource->tier);
    } else if (parent_composition != nullptr) {
      inherited_tint = ore_tint(*parent_composition);
    }
    child_geometries = fracture_asteroid_geometry(
        *parent_geometry, {.x = fracture_direction.x, .y = fracture_direction.y, .z = 0.35F}, fragment_count,
        child_radius, {.x = inherited_tint.r, .y = inherited_tint.g, .z = inherited_tint.b}, [&allocations] {
          std::vector<Vec3> tints;
          tints.reserve(allocations.size());
          for (const FragmentMaterialAllocation& allocation : allocations) {
            const OreTint tint = ore_tint(allocation.composition);
            tints.push_back({.x = tint.r, .y = tint.g, .z = tint.b});
          }
          return tints;
        }());
  }
  std::vector<entt::entity> fragments;
  fragments.reserve(allocations.size());

  for (int index = 0; index < fragment_count; ++index) {
    const FragmentMaterialAllocation& allocation = allocations[static_cast<std::size_t>(index)];
    const float angle =
        (static_cast<float>(index) / static_cast<float>(fragment_count)) * std::numbers::pi_v<float> * 2.0F;
    const Vec2 offset = direction_from_angle(angle) * placement_radius;
    const entt::entity fragment = registry.create();
    registry.emplace<AsteroidBody>(
        fragment, AsteroidBody{
                      .position = parent.position + offset,
                      .velocity = fragment_velocity(parent, request, index, fragment_count),
                      .radius = child_radius,
                      .base_radius = child_radius,
                      .rotation_radians = parent.rotation_radians + angle,
                      .angular_velocity = parent.angular_velocity + ((static_cast<float>(index) - 1.5F) * 0.12F),
                      .scan_confidence = parent.scan_confidence * 0.85F,
                  });
    registry.emplace<AsteroidFragmentation>(fragment,
                                            AsteroidFragmentation{.remaining_breaks = child_remaining_breaks});
    if (static_cast<std::size_t>(index) < child_geometries.size()) {
      registry.emplace<AsteroidGeometry>(fragment, std::move(child_geometries[static_cast<std::size_t>(index)]));
    }
    const float child_mass = parent_mass != nullptr ? parent_mass->remaining_mass * allocation.mass_fraction
                                                    : child_radius * allocation.mass_fraction;
    registry.emplace<AsteroidMass>(fragment, AsteroidMass{.initial_mass = child_mass, .remaining_mass = child_mass});
    if (parent_resource != nullptr) {
      std::array<float, OreTierCount> extracted_mass_by_tier{};
      for (std::size_t tier = 0; tier < extracted_mass_by_tier.size(); ++tier) {
        extracted_mass_by_tier[tier] = parent_resource->extracted_mass_by_tier[tier] * allocation.mass_fraction;
      }
      registry.emplace<MiningResource>(fragment,
                                       MiningResource{
                                           .tier = allocation.tier,
                                           .integrity = 100.0F,
                                           .extracted_mass = parent_resource->extracted_mass * allocation.mass_fraction,
                                           .extracted_mass_by_tier = extracted_mass_by_tier,
                                       });
    }
    registry.emplace<MineralComposition>(fragment, allocation.composition);
    fragments.push_back(fragment);
  }

  registry.destroy(asteroid);
  emit_fragmented(event_bus, asteroid, parent.position, static_cast<int>(fragments.size()), child_remaining_breaks);
  return fragments;
}

} // namespace

std::vector<entt::entity> fragment_asteroid(entt::registry& registry, entt::entity asteroid,
                                            const AsteroidFragmentationRequest& request) {
  return fragment_asteroid_impl(registry, nullptr, asteroid, request);
}

std::vector<entt::entity> fragment_asteroid(entt::registry& registry, DomainEventBus& event_bus, entt::entity asteroid,
                                            const AsteroidFragmentationRequest& request) {
  return fragment_asteroid_impl(registry, &event_bus, asteroid, request);
}

} // namespace hyperverse
