#pragma once

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

[[nodiscard]] std::vector<entt::entity> fragment_asteroid(
  entt::registry& registry,
  entt::entity asteroid,
  const AsteroidFragmentationRequest& request
);

}  // namespace hyperverse
