#include "hyperverse/drone.hpp"

#include <algorithm>
#include <cmath>

namespace hyperverse {
namespace {

constexpr float TauRadians = 6.28318530718F;

[[nodiscard]] bool valid_mining_target(entt::registry& registry, entt::entity target) {
  return target != entt::null && registry.valid(target) && registry.all_of<AsteroidBody, MiningResource>(target) &&
         registry.get<MiningResource>(target).integrity > 0.0F;
}

void emit_target_released(DomainEventBus* event_bus, entt::entity drone_entity, entt::entity target, Vec2 position) {
  if (event_bus == nullptr || target == entt::null) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::DroneTargetReleased,
    DomainEvent{
      .type = DomainEventType::DroneTargetReleased,
      .subject = drone_entity,
      .target = target,
      .position = position,
    }
  );
}

[[nodiscard]] Vec2 direction_from_angle(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

[[nodiscard]] Vec2 idle_formation_position(const MiningDrone& drone, const ShipMotion& ship, const SectorTuning& sector, const MiningDroneTuning& tuning) {
  const Vec2 forward = direction_from_angle(ship.facing_radians);
  const Vec2 right{.x = -forward.y, .y = forward.x};
  const float side_offset = std::sin(drone.work_angle_radians) * tuning.formation_spread;
  const float trail_offset = tuning.formation_trail_distance + (std::abs(std::cos(drone.work_angle_radians)) * tuning.formation_spread * 0.45F);
  return wrap_position(ship.position - (forward * trail_offset) + (right * side_offset), sector);
}

void update_facing_from_velocity(MiningDrone& drone, float dead_stick_speed) {
  if (length(drone.velocity) >= dead_stick_speed) {
    drone.facing_radians = std::atan2(drone.velocity.y, drone.velocity.x);
  }
}

}  // namespace

MiningDroneHudSnapshot update_mining_drone(
  MiningDrone& drone,
  entt::registry& registry,
  const TargetLockModel& mining_priority,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const MiningDroneTuning& tuning,
  DomainEventBus* event_bus
) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  drone.work_angle_radians = std::fmod(drone.work_angle_radians + (tuning.work_angle_rotation_radians_per_second * scaled_dt), TauRadians);
  if (drone.work_angle_radians < 0.0F) {
    drone.work_angle_radians += TauRadians;
  }

  if (has_locked_target(mining_priority) && valid_mining_target(registry, mining_priority.target)) {
    drone.target = mining_priority.target;
  } else if (!valid_mining_target(registry, drone.target)) {
    emit_target_released(event_bus, entt::null, drone.target, drone.position);
    drone.target = entt::null;
  }

  if (valid_mining_target(registry, drone.target)) {
    const AsteroidBody& target_body = registry.get<AsteroidBody>(drone.target);
    if (length(wrapped_delta(ship.position, target_body.position, sector)) > tuning.max_target_distance_from_ship) {
      emit_target_released(event_bus, entt::null, drone.target, drone.position);
      drone.target = entt::null;
    }
  }

  MiningDroneHudSnapshot hud{.phase = drone.phase, .target = drone.target, .extracted_mass = drone.extracted_mass};
  if (drone.target == entt::null) {
    const Vec2 formation_position = idle_formation_position(drone, ship, sector, tuning);
    const Vec2 to_formation = wrapped_delta(drone.position, formation_position, sector);
    drone.phase = MiningDronePhase::Idle;
    hud.target_distance = length(to_formation);
    if (hud.target_distance > tuning.arrival_tolerance) {
      drone.velocity = normalize_or_zero(to_formation) * tuning.max_speed;
      update_facing_from_velocity(drone, tuning.facing_dead_stick_speed);
      drone.position = wrap_position(drone.position + (drone.velocity * dt_seconds), sector);
    } else {
      drone.velocity = ship.velocity;
      update_facing_from_velocity(drone, tuning.facing_dead_stick_speed);
    }
    hud.phase = drone.phase;
    return hud;
  }

  AsteroidBody& asteroid = registry.get<AsteroidBody>(drone.target);
  MiningResource& resource = registry.get<MiningResource>(drone.target);
  const float work_radius = asteroid.radius + tuning.work_standoff;
  const Vec2 work_position = wrap_position(asteroid.position + (direction_from_angle(drone.work_angle_radians) * work_radius), sector);
  const Vec2 to_work_position = wrapped_delta(drone.position, work_position, sector);
  const Vec2 to_target = wrapped_delta(drone.position, asteroid.position, sector);
  hud.target_distance = length(to_target);

  if (length(to_work_position) > tuning.arrival_tolerance) {
    drone.phase = MiningDronePhase::Travelling;
    drone.velocity = normalize_or_zero(to_work_position) * tuning.max_speed;
    update_facing_from_velocity(drone, tuning.facing_dead_stick_speed);
    drone.position = wrap_position(drone.position + (drone.velocity * dt_seconds), sector);
  } else {
    drone.phase = MiningDronePhase::Mining;
    drone.velocity = {};
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
