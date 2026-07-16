#pragma once

#include "hyperverse/domain_events.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <vector>

namespace hyperverse {

enum class AsteroidImpactKind {
  Laser,
  Kinetic,
  Explosive,
};

struct AsteroidFragmentationRequest {
  AsteroidImpactKind impact_kind{AsteroidImpactKind::Kinetic};
  Vec2 impact_position{};
  Vec2 impact_velocity{};
  int pieces{4};
};

struct AsteroidFragmentation {
  int remaining_breaks{2};
};

inline constexpr float MinimumPlayableAsteroidRadius = 75.0F;

[[nodiscard]] std::vector<entt::entity> fragment_asteroid(
  entt::registry& registry,
  entt::entity asteroid,
  const AsteroidFragmentationRequest& request
);

[[nodiscard]] std::vector<entt::entity> fragment_asteroid(
  entt::registry& registry,
  DomainEventBus& event_bus,
  entt::entity asteroid,
  const AsteroidFragmentationRequest& request
);

}  // namespace hyperverse
