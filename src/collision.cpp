#include "hyperverse/collision.hpp"

#include "hyperverse/asteroid_collision.hpp"
#include "jolt_shape_queries.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <vector>
namespace hyperverse {

namespace {

struct CollisionCandidate {
  entt::entity entity{entt::null};
  AsteroidBody asteroid{};
  Vec2 relative_position{};
  float asteroid_radius{0.0F};
  float range{0.0F};
};

[[nodiscard]] std::size_t swept_check_budget(float speed, const CollisionProbe& probe) {
  const float zoom_scale = std::max(0.0F, probe.zoom_scale);
  const float scaled_budget =
    static_cast<float>(probe.min_swept_checks) + (speed * probe.speed_checks_per_world_unit) + (zoom_scale * probe.zoom_checks_per_scale);
  const std::size_t requested = static_cast<std::size_t>(std::ceil(std::max(0.0F, scaled_budget)));
  return std::clamp(requested, probe.min_swept_checks, probe.max_swept_checks);
}

}  // namespace

SweptCircleHit swept_circle_intersection(Vec2 relative_position, Vec2 motion, float combined_radius) {
  const float radius = std::max(0.0F, combined_radius);
  const float initial_distance = length(relative_position);
  const float separation = std::max(0.0F, initial_distance - radius);
  if (initial_distance <= radius) {
    return {.hit = true, .fraction = 0.0F, .separation = 0.0F};
  }

  const float motion_length_squared = dot(motion, motion);
  if (motion_length_squared <= 0.0001F) {
    return {.hit = false, .separation = separation};
  }

  const float projection = dot(relative_position, motion) / motion_length_squared;
  const float clamped_fraction = std::clamp(projection, 0.0F, 1.0F);
  const Vec2 closest_delta = relative_position - (motion * clamped_fraction);
  if (dot(closest_delta, closest_delta) <= radius * radius) {
    return {.hit = true, .fraction = clamped_fraction, .separation = separation};
  }

  return {.hit = false, .separation = separation};
}

CollisionHudSnapshot predict_ship_asteroid_collision(
  const ShipMotion& ship,
  entt::registry& registry,
  const SectorTuning& sector,
  const CollisionProbe& probe
) {
  CollisionHudSnapshot nearest{};
  bool found_hit = false;
  float best_time = probe.warning_seconds;
  const float speed = length(ship.velocity);
  const float scan_radius = std::max(0.0F, probe.view_radius_world * std::max(0.0F, probe.zoom_scale)) +
                            (speed * std::max(0.0F, probe.warning_seconds)) + probe.ship_radius;
  std::vector<CollisionCandidate> candidates;

  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    const Vec2 relative_position = wrapped_delta(ship.position, asteroid.position, sector);
    const float asteroid_radius = asteroid_solid_radius(asteroid.radius);
    const float range = std::max(0.0F, length(relative_position) - probe.ship_radius - asteroid_radius);
    if (range <= scan_radius + asteroid_radius) {
      candidates.push_back(CollisionCandidate{
        .entity = entity,
        .asteroid = asteroid,
        .relative_position = relative_position,
        .asteroid_radius = asteroid_radius,
        .range = range,
      });
    }
  }

  std::ranges::sort(candidates, [](const CollisionCandidate& lhs, const CollisionCandidate& rhs) {
    return lhs.range < rhs.range;
  });
  const std::size_t budget = swept_check_budget(speed, probe);
  if (candidates.size() > budget) {
    candidates.resize(budget);
  }
  const std::size_t candidate_count = candidates.size();
  std::size_t swept_checks = 0U;

  for (const CollisionCandidate& candidate_data : candidates) {
    const AsteroidBody& asteroid = candidate_data.asteroid;
    const Vec2 relative_position = candidate_data.relative_position;
    const Vec2 relative_velocity = asteroid.velocity - ship.velocity;
    const Vec2 ship_motion = (ship.velocity - asteroid.velocity) * probe.warning_seconds;
    const float motion_length = length(ship_motion);
    const float asteroid_radius = candidate_data.asteroid_radius;
    const float combined_radius = probe.ship_radius + asteroid_radius;
    const bool contact =
      jolt_shapes_overlap(SpriteCollisionShape::Ship, probe.ship_radius, SpriteCollisionShape::Rock, asteroid_radius, relative_position);
    CollisionHudSnapshot candidate{.contact = contact, .warning = contact, .asteroid = candidate_data.entity};
    if (contact) {
      candidate.impact_speed = length(relative_velocity);
    } else if (motion_length > 0.0001F) {
      ++swept_checks;
      const SweptCircleHit swept_hit = swept_circle_intersection(relative_position, ship_motion, combined_radius);
      if (swept_hit.hit) {
        const ShapeQueryHit hit =
          jolt_cast_shape(SpriteCollisionShape::Ship, probe.ship_radius, SpriteCollisionShape::Rock, asteroid_radius, relative_position, ship_motion);
        if (hit.hit) {
          candidate.warning = true;
          candidate.separation = hit.fraction * motion_length;
          candidate.time_to_contact_seconds = hit.fraction * probe.warning_seconds;
          candidate.impact_speed = motion_length / probe.warning_seconds;
        }
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

  nearest.candidate_count = candidate_count;
  nearest.swept_checks = swept_checks;
  return nearest;
}

}  // namespace hyperverse
