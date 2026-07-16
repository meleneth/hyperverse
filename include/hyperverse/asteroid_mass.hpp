#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

struct AsteroidMass {
  float initial_mass{0.0F};
  float remaining_mass{0.0F};
};

[[nodiscard]] AsteroidMass asteroid_mass_from_radius(float radius);
float extract_asteroid_mass(entt::registry& registry, entt::entity asteroid, float requested_mass);

}  // namespace hyperverse
