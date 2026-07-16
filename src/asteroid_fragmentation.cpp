#include "hyperverse/asteroid_fragmentation.hpp"

#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/mining.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace hyperverse {
namespace {

[[nodiscard]] Vec2 perpendicular(Vec2 value) {
  return {.x = -value.y, .y = value.x};
}

[[nodiscard]] Vec2 direction_from_angle(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
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

[[nodiscard]] Vec2 fragment_velocity(
  const AsteroidBody& parent,
  const AsteroidFragmentationRequest& request,
  int index,
  int pieces
) {
  const Vec2 forward = impact_direction(request, parent);
  const Vec2 tangent = perpendicular(forward);
  const float impact_speed = length(request.impact_velocity);
  const float centered_index = static_cast<float>(index) - ((static_cast<float>(pieces) - 1.0F) * 0.5F);

  switch (request.impact_kind) {
    case AsteroidImpactKind::Laser:
      return parent.velocity + (forward * std::clamp(impact_speed * 0.04F, 8.0F, 70.0F)) + (tangent * centered_index * 4.0F);
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
  event_bus->enqueue(
    DomainEventType::AsteroidConsumed,
    DomainEvent{
      .type = DomainEventType::AsteroidConsumed,
      .subject = asteroid,
      .position = position,
    }
  );
}

void emit_fragmented(
  DomainEventBus* event_bus,
  entt::entity asteroid,
  Vec2 position,
  int fragment_count,
  int child_remaining_breaks
) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::AsteroidFragmented,
    DomainEvent{
      .type = DomainEventType::AsteroidFragmented,
      .subject = asteroid,
      .position = position,
      .amount = static_cast<float>(child_remaining_breaks),
      .count = fragment_count,
    }
  );
}

[[nodiscard]] std::vector<entt::entity> fragment_asteroid_impl(
  entt::registry& registry,
  DomainEventBus* event_bus,
  entt::entity asteroid,
  const AsteroidFragmentationRequest& request
) {
  if (!registry.valid(asteroid) || !registry.all_of<AsteroidBody>(asteroid) || request.pieces < 2) {
    return {};
  }

  const AsteroidBody parent = registry.get<AsteroidBody>(asteroid);
  const int parent_remaining_breaks = remaining_breaks_for(registry, asteroid);
  const float child_radius = std::max(8.0F, parent.radius / std::sqrt(static_cast<float>(request.pieces)));
  if (parent_remaining_breaks <= 0 || child_radius < MinimumPlayableAsteroidRadius) {
    registry.destroy(asteroid);
    emit_consumed(event_bus, asteroid, parent.position);
    return {};
  }

  const MiningResource* parent_resource = registry.try_get<MiningResource>(asteroid);
  const MineralComposition* parent_composition = registry.try_get<MineralComposition>(asteroid);
  const AsteroidMass* parent_mass = registry.try_get<AsteroidMass>(asteroid);
  const int child_remaining_breaks = parent_remaining_breaks - 1;
  const float placement_radius = std::max(child_radius, parent.radius - child_radius);
  std::vector<entt::entity> fragments;
  fragments.reserve(static_cast<std::size_t>(request.pieces));

  for (int index = 0; index < request.pieces; ++index) {
    const float angle = (static_cast<float>(index) / static_cast<float>(request.pieces)) * std::numbers::pi_v<float> * 2.0F;
    const Vec2 offset = direction_from_angle(angle) * placement_radius;
    const entt::entity fragment = registry.create();
    registry.emplace<AsteroidBody>(
      fragment,
      AsteroidBody{
        .position = parent.position + offset,
        .velocity = fragment_velocity(parent, request, index, request.pieces),
        .radius = child_radius,
        .base_radius = child_radius,
        .rotation_radians = parent.rotation_radians + angle,
        .angular_velocity = parent.angular_velocity + ((static_cast<float>(index) - 1.5F) * 0.12F),
        .scan_confidence = parent.scan_confidence,
      }
    );
    registry.emplace<AsteroidFragmentation>(fragment, AsteroidFragmentation{.remaining_breaks = child_remaining_breaks});
    const float child_mass = parent_mass != nullptr ? parent_mass->remaining_mass / static_cast<float>(request.pieces) : child_radius;
    registry.emplace<AsteroidMass>(fragment, AsteroidMass{.initial_mass = child_mass, .remaining_mass = child_mass});
    if (parent_resource != nullptr) {
      registry.emplace<MiningResource>(
        fragment,
        MiningResource{
          .tier = parent_resource->tier,
          .integrity = 100.0F,
          .extracted_mass = parent_resource->extracted_mass / static_cast<float>(request.pieces),
        }
      );
    }
    if (parent_composition != nullptr) {
      registry.emplace<MineralComposition>(fragment, *parent_composition);
    }
    fragments.push_back(fragment);
  }

  registry.destroy(asteroid);
  emit_fragmented(event_bus, asteroid, parent.position, static_cast<int>(fragments.size()), child_remaining_breaks);
  return fragments;
}

}  // namespace

std::vector<entt::entity> fragment_asteroid(
  entt::registry& registry,
  entt::entity asteroid,
  const AsteroidFragmentationRequest& request
) {
  return fragment_asteroid_impl(registry, nullptr, asteroid, request);
}

std::vector<entt::entity> fragment_asteroid(
  entt::registry& registry,
  DomainEventBus& event_bus,
  entt::entity asteroid,
  const AsteroidFragmentationRequest& request
) {
  return fragment_asteroid_impl(registry, &event_bus, asteroid, request);
}

}  // namespace hyperverse
