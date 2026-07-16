#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

struct AsteroidMass {
  float initial_mass{0.0F};
  float remaining_mass{0.0F};
};

[[nodiscard]] AsteroidMass asteroid_mass_from_radius(float radius);
void sync_asteroid_mass_to_integrity(entt::registry& registry, entt::entity asteroid, float integrity_fraction);

}  // namespace hyperverse
