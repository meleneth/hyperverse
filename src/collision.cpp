#include "hyperverse/collision.hpp"

#include <algorithm>
#include <limits>

namespace hyperverse {

CollisionHudSnapshot predict_ship_asteroid_collision(
  const ShipMotion& ship,
  entt::registry& registry,
  const SectorTuning& sector,
  const CollisionProbe& probe
) {
  CollisionHudSnapshot nearest{};
  float nearest_time = std::numeric_limits<float>::max();
  float nearest_separation = std::numeric_limits<float>::max();

  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    const Vec2 relative_position = wrapped_delta(ship.position, asteroid.position, sector);
    const Vec2 relative_velocity = asteroid.velocity - ship.velocity;
    const float center_distance = length(relative_position);
    const float combined_radius = probe.ship_radius + asteroid.radius;
    const float separation = center_distance - combined_radius;
    const Vec2 direction = normalize_or_zero(relative_position);
    const float closing_speed = std::max(0.0F, -dot(direction, relative_velocity));

    CollisionHudSnapshot candidate{
      .contact = separation <= 0.0F,
      .warning = false,
      .asteroid = entity,
      .separation = std::max(0.0F, separation),
      .impact_speed = closing_speed,
      .time_to_contact_seconds = closing_speed > 0.0001F ? std::max(0.0F, separation) / closing_speed : 0.0F,
    };
    candidate.warning = candidate.contact || (closing_speed > 0.0F && candidate.time_to_contact_seconds <= probe.warning_seconds);

    const bool better_contact = candidate.contact && !nearest.contact;
    const bool better_warning = candidate.warning && !nearest.warning;
    const bool sooner_warning = candidate.warning && nearest.warning && candidate.time_to_contact_seconds < nearest_time;
    const bool nearer_idle = !nearest.warning && !candidate.warning && candidate.separation < nearest_separation;
    if (better_contact || better_warning || sooner_warning || nearer_idle) {
      nearest = candidate;
      nearest_time = candidate.time_to_contact_seconds;
      nearest_separation = candidate.separation;
    }
  }

  return nearest;
}

}  // namespace hyperverse
