#include "hyperverse/collision.hpp"

#include "sphere_queries.hpp"

#include <algorithm>
namespace hyperverse {

CollisionHudSnapshot predict_ship_asteroid_collision(
  const ShipMotion& ship,
  entt::registry& registry,
  const SectorTuning& sector,
  const CollisionProbe& probe
) {
  CollisionHudSnapshot nearest{};
  bool found_hit = false;
  float best_time = probe.warning_seconds;

  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    const Vec2 relative_position = wrapped_delta(ship.position, asteroid.position, sector);
    const Vec2 relative_velocity = asteroid.velocity - ship.velocity;
    const Vec2 ship_motion = (ship.velocity - asteroid.velocity) * probe.warning_seconds;
    const float motion_length = length(ship_motion);
    const float combined_radius = probe.ship_radius + asteroid.radius;

    const bool contact = circles_overlap(relative_position, combined_radius);
    CollisionHudSnapshot candidate{.contact = contact, .warning = contact, .asteroid = entity};
    if (contact) {
      candidate.impact_speed = length(relative_velocity);
    } else if (motion_length > 0.0001F) {
      const SphereCastHit hit = cast_circle(relative_position, ship_motion, combined_radius);
      if (hit.hit) {
        candidate.warning = true;
        candidate.separation = hit.fraction * motion_length;
        candidate.time_to_contact_seconds = hit.fraction * probe.warning_seconds;
        candidate.impact_speed = motion_length / probe.warning_seconds;
      }
    }

    const bool better_contact = candidate.contact && !nearest.contact;
    const bool first_hit = candidate.warning && !found_hit;
    const bool sooner_hit = candidate.warning && found_hit && candidate.time_to_contact_seconds < best_time;
    if (better_contact || first_hit || sooner_hit) {
      nearest = candidate;
      found_hit = candidate.warning;
      best_time = candidate.time_to_contact_seconds;
    }
  }

  return nearest;
}

}  // namespace hyperverse
