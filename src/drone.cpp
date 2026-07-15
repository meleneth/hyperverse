#include "hyperverse/drone.hpp"

#include <algorithm>

namespace hyperverse {
namespace {

[[nodiscard]] bool valid_mining_target(entt::registry& registry, entt::entity target) {
  return target != entt::null && registry.valid(target) && registry.all_of<AsteroidBody, MiningResource>(target) &&
         registry.get<MiningResource>(target).integrity > 0.0F;
}

}  // namespace

MiningDroneHudSnapshot update_mining_drone(
  MiningDrone& drone,
  entt::registry& registry,
  const TargetLockModel& mining_priority,
  const SectorTuning& sector,
  float dt_seconds,
  const MiningDroneTuning& tuning
) {
  if (has_locked_target(mining_priority) && valid_mining_target(registry, mining_priority.target)) {
    drone.target = mining_priority.target;
  } else if (!valid_mining_target(registry, drone.target)) {
    drone.target = entt::null;
  }

  MiningDroneHudSnapshot hud{.phase = drone.phase, .target = drone.target, .extracted_mass = drone.extracted_mass};
  if (drone.target == entt::null) {
    drone.phase = MiningDronePhase::Idle;
    drone.velocity = {};
    hud.phase = drone.phase;
    return hud;
  }

  AsteroidBody& asteroid = registry.get<AsteroidBody>(drone.target);
  MiningResource& resource = registry.get<MiningResource>(drone.target);
  const Vec2 to_target = wrapped_delta(drone.position, asteroid.position, sector);
  const float distance = length(to_target);
  hud.target_distance = distance;

  if (distance > tuning.mining_range) {
    drone.phase = MiningDronePhase::Travelling;
    drone.velocity = normalize_or_zero(to_target) * tuning.max_speed;
    drone.position = wrap_position(drone.position + (drone.velocity * dt_seconds), sector);
  } else {
    drone.phase = MiningDronePhase::Mining;
    drone.velocity = {};
    const float scaled_dt = std::max(0.0F, dt_seconds);
    resource.integrity = std::max(0.0F, resource.integrity - (tuning.integrity_damage_per_second * scaled_dt));
    resource.extracted_mass += tuning.extraction_per_second * scaled_dt;
    drone.extracted_mass += tuning.extraction_per_second * scaled_dt;
  }

  hud.phase = drone.phase;
  hud.target = drone.target;
  hud.extracted_mass = drone.extracted_mass;
  return hud;
}

}  // namespace hyperverse
