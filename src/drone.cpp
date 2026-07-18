#include "hyperverse/drone.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/cargo_box.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {
namespace {

constexpr float TauRadians = 6.28318530718F;
namespace sml = boost::sml;

struct drone_unassigned {};
struct drone_pickup_cargo {};
struct drone_escorting_cargo {};
struct cargo_assigned {};
struct cargo_picked_up {};
struct cargo_delivered {};

struct DroneCargoMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<drone_unassigned> + event<cargo_assigned> = state<drone_pickup_cargo>,
      state<drone_pickup_cargo> + event<cargo_picked_up> = state<drone_escorting_cargo>,
      state<drone_escorting_cargo> + event<cargo_delivered> = state<drone_unassigned>
    );
  }
};

void replay_cargo_phase(sml::sm<DroneCargoMachine>& machine, MiningDronePhase phase, entt::entity cargo_target) {
  if (cargo_target == entt::null) {
    return;
  }
  machine.process_event(cargo_assigned{});
  if (phase == MiningDronePhase::EscortingCargo) {
    machine.process_event(cargo_picked_up{});
  }
}

[[nodiscard]] MiningDronePhase read_cargo_phase(const sml::sm<DroneCargoMachine>& machine) {
  if (machine.is(sml::state<drone_escorting_cargo>)) {
    return MiningDronePhase::EscortingCargo;
  }
  if (machine.is(sml::state<drone_pickup_cargo>)) {
    return MiningDronePhase::CargoPickup;
  }
  return MiningDronePhase::Idle;
}

[[nodiscard]] bool valid_mining_target(entt::registry& registry, entt::entity target) {
  if (
    target == entt::null || !registry.valid(target) || !registry.all_of<AsteroidBody, MiningResource>(target) ||
    registry.get<MiningResource>(target).integrity <= 0.0F
  ) {
    return false;
  }
  const AsteroidFragmentation* fragmentation = registry.try_get<AsteroidFragmentation>(target);
  return fragmentation == nullptr || fragmentation->remaining_breaks <= 1;
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

void emit_cargo_pickup_started(DomainEventBus* event_bus, entt::entity drone_entity, entt::entity box, Vec2 position) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoBoxPickupStarted,
    DomainEvent{.type = DomainEventType::CargoBoxPickupStarted, .subject = drone_entity, .target = box, .position = position}
  );
}

void emit_cargo_delivered(DomainEventBus* event_bus, entt::entity drone_entity, entt::entity box, Vec2 position) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoBoxDeliveredToGathering,
    DomainEvent{.type = DomainEventType::CargoBoxDeliveredToGathering, .subject = drone_entity, .target = box, .position = position}
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

[[nodiscard]] bool valid_cargo_target(entt::registry& registry, entt::entity target) {
  if (target == entt::null || !registry.valid(target) || !registry.all_of<CargoBox>(target)) {
    return false;
  }
  const CargoBox& box = registry.get<CargoBox>(target);
  return box.state == CargoBoxState::PendingPickup || box.state == CargoBoxState::BeingHauled;
}

void move_drone_toward(MiningDrone& drone, Vec2 target, const SectorTuning& sector, float dt_seconds, const MiningDroneTuning& tuning) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const Vec2 delta = wrapped_delta(drone.position, target, sector);
  const float distance = length(delta);
  if (distance <= 0.001F || scaled_dt <= 0.0F) {
    drone.velocity = {};
    return;
  }

  const Vec2 desired_velocity = normalize_or_zero(delta) * tuning.max_speed;
  const Vec2 step = desired_velocity * scaled_dt;
  if (length(step) >= distance) {
    drone.velocity = delta * (1.0F / std::max(scaled_dt, std::numeric_limits<float>::epsilon()));
    drone.position = target;
  } else {
    drone.velocity = desired_velocity;
    drone.position = wrap_position(drone.position + step, sector);
  }
  update_facing_from_velocity(drone, tuning.facing_dead_stick_speed);
}

[[nodiscard]] bool update_cargo_haul(
  MiningDrone& drone,
  entt::registry& registry,
  const SectorTuning& sector,
  float dt_seconds,
  const MiningDroneTuning& tuning,
  DomainEventBus* event_bus,
  MiningDroneHudSnapshot& hud
) {
  if (!valid_cargo_target(registry, drone.cargo_target)) {
    drone.cargo_target = entt::null;
    return false;
  }

  sml::sm<DroneCargoMachine> machine;
  replay_cargo_phase(machine, drone.phase, drone.cargo_target);
  CargoBox& box = registry.get<CargoBox>(drone.cargo_target);
  if (box.state == CargoBoxState::PendingPickup) {
    const Vec2 to_box = wrapped_delta(drone.position, box.position, sector);
    const float distance = length(to_box);
    drone.phase = MiningDronePhase::CargoPickup;
    hud.phase = drone.phase;
    hud.target = drone.cargo_target;
    hud.target_distance = distance;
    if (distance > tuning.cargo_pickup_tolerance) {
      move_drone_toward(drone, box.position, sector, dt_seconds, tuning);
    } else {
      box.state = CargoBoxState::BeingHauled;
      machine.process_event(cargo_picked_up{});
      drone.phase = read_cargo_phase(machine);
      emit_cargo_pickup_started(event_bus, entt::null, drone.cargo_target, box.position);
    }
    return true;
  }

  const Vec2 delivery_position = drone.cargo_destination;
  const Vec2 to_delivery = wrapped_delta(drone.position, delivery_position, sector);
  const float distance = length(to_delivery);
  drone.phase = MiningDronePhase::EscortingCargo;
  hud.phase = drone.phase;
  hud.target = drone.cargo_target;
  hud.target_distance = distance;
  if (distance > tuning.cargo_delivery_tolerance) {
    move_drone_toward(drone, delivery_position, sector, dt_seconds, tuning);
    box.position = drone.position;
    box.velocity = drone.velocity;
  } else {
    box.state = CargoBoxState::Linked;
    box.velocity = {};
    emit_cargo_delivered(event_bus, entt::null, drone.cargo_target, delivery_position);
    machine.process_event(cargo_delivered{});
    drone.cargo_target = entt::null;
    drone.cargo_destination = {};
    drone.phase = read_cargo_phase(machine);
  }
  return true;
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

  MiningDroneHudSnapshot hud{.phase = drone.phase, .target = drone.target, .extracted_mass = drone.extracted_mass};
  if (update_cargo_haul(drone, registry, sector, scaled_dt, tuning, event_bus, hud)) {
    return hud;
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
    hud.target = drone.target;
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
    const float extracted_mass = extract_asteroid_mass(registry, drone.target, tuning.extraction_per_second * scaled_dt);
    resource.extracted_mass += extracted_mass;
    drone.extracted_mass += extracted_mass;
  }

  hud.phase = drone.phase;
  hud.target = drone.target;
  hud.extracted_mass = drone.extracted_mass;
  return hud;
}

}  // namespace hyperverse
