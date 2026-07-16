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

void sync_asteroid_mass_to_integrity(entt::registry& registry, entt::entity asteroid, float integrity_fraction) {
  AsteroidMass* mass = registry.try_get<AsteroidMass>(asteroid);
  if (mass == nullptr) {
    return;
  }
  mass->remaining_mass = std::clamp(mass->initial_mass * std::clamp(integrity_fraction, 0.0F, 1.0F), 0.0F, mass->initial_mass);
}

}  // namespace hyperverse
