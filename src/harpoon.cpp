#include "hyperverse/harpoon.hpp"

#include <algorithm>

namespace hyperverse {
namespace {

[[nodiscard]] bool valid_harpoon_target(entt::registry& registry, entt::entity target) {
  return target != entt::null && registry.valid(target) && registry.all_of<AsteroidBody>(target);
}

[[nodiscard]] Vec2 surface_velocity_at_ship(const AsteroidBody& asteroid, const ShipMotion& ship, const SectorTuning& sector) {
  const Vec2 radial = normalize_or_zero(wrapped_delta(asteroid.position, ship.position, sector));
  const Vec2 tangent{.x = -radial.y, .y = radial.x};
  return asteroid.velocity + (tangent * asteroid.angular_velocity * asteroid.radius);
}

}  // namespace

HarpoonHudSnapshot update_harpoon(
  HarpoonModel& model,
  entt::registry& registry,
  const TargetLockModel& target_lock,
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const HarpoonTuning& tuning
) {
  if (input.boost_requested && model.phase == HarpoonPhase::Latched) {
    model = {};
    return {};
  }

  if (input.harpoon_requested) {
    if (model.phase == HarpoonPhase::Latched) {
      model = {};
    } else if (has_locked_target(target_lock) && valid_harpoon_target(registry, target_lock.target) && target_lock.wrapped_distance <= tuning.latch_range) {
      model.phase = HarpoonPhase::Latched;
      model.target = target_lock.target;
    }
  }

  HarpoonHudSnapshot hud{.latched = model.phase == HarpoonPhase::Latched, .target = model.target};
  if (model.phase != HarpoonPhase::Latched || !valid_harpoon_target(registry, model.target)) {
    model = {};
    return {};
  }

  AsteroidBody& asteroid = registry.get<AsteroidBody>(model.target);
  const float distance = wrapped_distance(ship.position, asteroid.position, sector);
  if (distance > tuning.release_range) {
    model = {};
    return {};
  }

  const Vec2 velocity_delta = ship.velocity - asteroid.velocity;
  asteroid.velocity += clamp_length(velocity_delta, tuning.asteroid_brake_per_second * std::max(0.0F, dt_seconds));
  const Vec2 ship_tow_velocity = surface_velocity_at_ship(asteroid, ship, sector);
  ship.velocity += clamp_length(ship_tow_velocity - ship.velocity, tuning.ship_pull_per_second * std::max(0.0F, dt_seconds));

  hud.latched = true;
  hud.target = model.target;
  hud.target_distance = distance;
  hud.target_speed = length(asteroid.velocity);
  return hud;
}

}  // namespace hyperverse
