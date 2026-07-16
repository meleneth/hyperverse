#include "hyperverse/asteroid_mass.hpp"

#include <algorithm>

namespace hyperverse {
namespace {

constexpr float AsteroidMassReferenceRadius = 80.0F;

}  // namespace

AsteroidMass asteroid_mass_from_radius(float radius) {
  const float clamped_radius = std::max(0.0F, radius);
  const float mass = (clamped_radius * clamped_radius) / AsteroidMassReferenceRadius;
  return {.initial_mass = mass, .remaining_mass = mass};
}

float extract_asteroid_mass(entt::registry& registry, entt::entity asteroid, float requested_mass) {
  const float clamped_request = std::max(0.0F, requested_mass);
  AsteroidMass* mass = registry.try_get<AsteroidMass>(asteroid);
  if (mass == nullptr) {
    return clamped_request;
  }
  const float extracted = std::min(mass->remaining_mass, clamped_request);
  mass->remaining_mass -= extracted;
  return extracted;
}

}  // namespace hyperverse
