#include "hyperverse/drone.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_collision.hpp"
#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/cargo_box.hpp"
#include "hyperverse/engine_trail.hpp"
#include "hyperverse/profiling.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <cmath>

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
struct drone_idle {};
struct drone_travelling {};
struct drone_mining {};
struct return_to_formation {};
struct travel_to_work {};
struct begin_mining {};
struct presence_spawn_rolling {};
struct presence_following {};
struct presence_exit_rolling {};
struct presence_hidden {};
struct presence_destroyed {};
struct spawn_requested {};
struct exit_requested {};
struct barrel_roll_completed {};
struct destroyed {};

struct DronePresenceMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
        *state<presence_spawn_rolling> + event<barrel_roll_completed> = state<presence_following>,
        state<presence_following> + event<exit_requested> = state<presence_exit_rolling>,
        state<presence_spawn_rolling> + event<exit_requested> = state<presence_exit_rolling>,
        state<presence_exit_rolling> + event<barrel_roll_completed> = state<presence_hidden>,
        state<presence_hidden> + event<spawn_requested> = state<presence_spawn_rolling>,
        state<presence_following> + event<destroyed> = state<presence_destroyed>,
        state<presence_spawn_rolling> + event<destroyed> = state<presence_destroyed>,
        state<presence_destroyed> + event<spawn_requested> = state<presence_spawn_rolling>,
        state<presence_following> + event<spawn_requested> = state<presence_spawn_rolling>,
        state<presence_exit_rolling> + event<spawn_requested> = state<presence_spawn_rolling>,
        state<presence_spawn_rolling> + event<spawn_requested> = state<presence_spawn_rolling>);
  }
};

void replay_presence(sml::sm<DronePresenceMachine>& machine, DronePresencePhase phase) {
  switch (phase) {
  case DronePresencePhase::SpawnBarrelRoll:
    return;
  case DronePresencePhase::Following:
    machine.process_event(barrel_roll_completed{});
    return;
  case DronePresencePhase::ExitBarrelRoll:
    machine.process_event(barrel_roll_completed{});
    machine.process_event(exit_requested{});
    return;
  case DronePresencePhase::Hidden:
    machine.process_event(barrel_roll_completed{});
    machine.process_event(exit_requested{});
    machine.process_event(barrel_roll_completed{});
    return;
  case DronePresencePhase::DestroyedAwaitingRespawn:
    machine.process_event(barrel_roll_completed{});
    machine.process_event(destroyed{});
    return;
  }
}

DronePresencePhase read_presence(const sml::sm<DronePresenceMachine>& machine) {
  if (machine.is(sml::state<presence_following>))
    return DronePresencePhase::Following;
  if (machine.is(sml::state<presence_exit_rolling>))
    return DronePresencePhase::ExitBarrelRoll;
  if (machine.is(sml::state<presence_hidden>))
    return DronePresencePhase::Hidden;
  if (machine.is(sml::state<presence_destroyed>))
    return DronePresencePhase::DestroyedAwaitingRespawn;
  return DronePresencePhase::SpawnBarrelRoll;
}

struct DroneCargoMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(*state<drone_unassigned> + event<cargo_assigned> = state<drone_pickup_cargo>,
                                 state<drone_pickup_cargo> + event<cargo_picked_up> = state<drone_escorting_cargo>,
                                 state<drone_escorting_cargo> + event<cargo_delivered> = state<drone_unassigned>);
  }
};

struct DroneWorkMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(*state<drone_idle> + event<travel_to_work> = state<drone_travelling>,
                                 state<drone_idle> + event<begin_mining> = state<drone_mining>,
                                 state<drone_travelling> + event<return_to_formation> = state<drone_idle>,
                                 state<drone_travelling> + event<begin_mining> = state<drone_mining>,
                                 state<drone_mining> + event<return_to_formation> = state<drone_idle>,
                                 state<drone_mining> + event<travel_to_work> = state<drone_travelling>);
  }
};

void replay_work_phase(sml::sm<DroneWorkMachine>& machine, MiningDronePhase phase) {
  switch (phase) {
  case MiningDronePhase::Idle:
  case MiningDronePhase::CargoPickup:
  case MiningDronePhase::EscortingCargo:
    return;
  case MiningDronePhase::Travelling:
    machine.process_event(travel_to_work{});
    return;
  case MiningDronePhase::Mining:
    machine.process_event(begin_mining{});
    return;
  }
}

[[nodiscard]] MiningDronePhase read_work_phase(const sml::sm<DroneWorkMachine>& machine) {
  if (machine.is(sml::state<drone_mining>)) {
    return MiningDronePhase::Mining;
  }
  if (machine.is(sml::state<drone_travelling>)) {
    return MiningDronePhase::Travelling;
  }
  return MiningDronePhase::Idle;
}

void replay_cargo_phase(sml::sm<DroneCargoMachine>& machine, MiningDronePhase phase, entt::entity cargo_target) {
  (void)cargo_target;
  switch (phase) {
  case MiningDronePhase::Idle:
  case MiningDronePhase::Travelling:
  case MiningDronePhase::Mining:
    return;
  case MiningDronePhase::CargoPickup:
    machine.process_event(cargo_assigned{});
    return;
  case MiningDronePhase::EscortingCargo:
    machine.process_event(cargo_assigned{});
    machine.process_event(cargo_picked_up{});
    return;
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
  if (target == entt::null || !registry.valid(target) || !registry.all_of<AsteroidBody, MiningResource>(target) ||
      registry.get<MiningResource>(target).integrity <= 0.0F) {
    return false;
  }
  const AsteroidFragmentation* fragmentation = registry.try_get<AsteroidFragmentation>(target);
  return fragmentation == nullptr || fragmentation->remaining_breaks <= 1;
}

void emit_target_released(DomainEventBus* event_bus, entt::entity drone_entity, entt::entity target, Vec2 position) {
  if (event_bus == nullptr || target == entt::null) {
    return;
  }
  event_bus->enqueue(DomainEventType::DroneTargetReleased, DomainEvent{
                                                               .type = DomainEventType::DroneTargetReleased,
                                                               .subject = drone_entity,
                                                               .target = target,
                                                               .position = position,
                                                           });
}

void emit_cargo_pickup_started(DomainEventBus* event_bus, entt::entity drone_entity, entt::entity box, Vec2 position) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(DomainEventType::CargoBoxPickupStarted, DomainEvent{.type = DomainEventType::CargoBoxPickupStarted,
                                                                         .subject = drone_entity,
                                                                         .target = box,
                                                                         .position = position});
}

void emit_cargo_delivered(DomainEventBus* event_bus, entt::entity drone_entity, entt::entity box, Vec2 position) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(DomainEventType::CargoBoxDeliveredToGathering,
                     DomainEvent{.type = DomainEventType::CargoBoxDeliveredToGathering,
                                 .subject = drone_entity,
                                 .target = box,
                                 .position = position});
}

[[nodiscard]] Vec2 direction_from_angle(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

[[nodiscard]] Vec2 idle_formation_position(const MiningDrone& drone, const ShipMotion& ship, const SectorTuning& sector,
                                           const MiningDroneTuning& tuning) {
  const Vec2 forward = direction_from_angle(ship.facing_radians);
  const Vec2 right{.x = -forward.y, .y = forward.x};
  const float side_offset = std::sin(drone.work_angle_radians) * tuning.formation_spread;
  const float trail_offset = tuning.formation_trail_distance +
                             (std::abs(std::cos(drone.work_angle_radians)) * tuning.formation_spread * 0.45F);
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

[[nodiscard]] Vec2 drone_avoidance(
  const MiningDrone& drone,
  entt::registry& registry,
  Vec2 planned_velocity,
  const SectorTuning& sector,
  const MiningDroneTuning& tuning
) {
  Vec2 steering{};
  entt::entity self = entt::null;
  for (auto [entity, candidate] : registry.view<MiningDrone>().each()) {
    if (&candidate == &drone) {
      self = entity;
      break;
    }
  }
  const float asteroid_horizon = std::max(0.0F, tuning.asteroid_avoidance_lookahead_seconds);
  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    (void)entity;
    const Vec2 relative_position = wrapped_delta(drone.position, asteroid.position, sector);
    const Vec2 relative_velocity = asteroid.velocity - planned_velocity;
    const float speed_squared = dot(relative_velocity, relative_velocity);
    const float closest_seconds = speed_squared > 0.0001F
                                    ? std::clamp(-dot(relative_position, relative_velocity) / speed_squared, 0.0F, asteroid_horizon)
                                    : 0.0F;
    const Vec2 closest_delta = relative_position + (relative_velocity * closest_seconds);
    const float safe_radius = asteroid_solid_radius(asteroid.radius) + std::max(0.0F, tuning.asteroid_avoidance_clearance);
    const float closest_distance = length(closest_delta);
    if (closest_distance >= safe_radius) {
      continue;
    }
    Vec2 away = normalize_or_zero(closest_delta) * -1.0F;
    if (length(away) <= 0.0001F) {
      away = normalize_or_zero(Vec2{.x = -relative_velocity.y, .y = relative_velocity.x});
    }
    steering += away * (1.0F - (closest_distance / std::max(safe_radius, 0.001F))) * tuning.asteroid_avoidance_weight;
  }

  const float separation_radius = std::max(0.0F, tuning.separation_radius);
  const float separation_horizon = std::max(0.0F, tuning.separation_lookahead_seconds);
  for (auto [entity, other] : registry.view<MiningDrone>().each()) {
    (void)entity;
    if (&other == &drone || other.integrity <= 0.0F) {
      continue;
    }
    const Vec2 relative_position = wrapped_delta(drone.position, other.position, sector);
    const Vec2 relative_velocity = other.velocity - planned_velocity;
    const float speed_squared = dot(relative_velocity, relative_velocity);
    const float closest_seconds = speed_squared > 0.0001F
                                    ? std::clamp(-dot(relative_position, relative_velocity) / speed_squared, 0.0F, separation_horizon)
                                    : 0.0F;
    const Vec2 closest_delta = relative_position + (relative_velocity * closest_seconds);
    const float closest_distance = length(closest_delta);
    if (closest_distance >= separation_radius) {
      continue;
    }
    Vec2 away = normalize_or_zero(closest_delta) * -1.0F;
    if (length(away) <= 0.0001F) {
      away = entt::to_integral(self) < entt::to_integral(entity) ? Vec2{.x = 0.0F, .y = -1.0F} : Vec2{.x = 0.0F, .y = 1.0F};
    }
    steering += away * (1.0F - (closest_distance / std::max(separation_radius, 0.001F))) * tuning.separation_weight;
  }
  return steering;
}

void move_drone_toward(MiningDrone& drone, entt::registry& registry, Vec2 target, Vec2 target_velocity,
                       const SectorTuning& sector, float dt_seconds, const MiningDroneTuning& tuning) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const Vec2 delta = wrapped_delta(drone.position, target, sector);
  const float distance = length(delta);
  if (scaled_dt <= 0.0F) {
    return;
  }

  const float acceleration = std::max(0.0F, tuning.acceleration);
  const float stopping_speed = std::sqrt(std::max(0.0F, 2.0F * acceleration * distance));
  const float approach_speed = std::min(std::max(0.0F, tuning.max_speed), stopping_speed);
  Vec2 desired_velocity = target_velocity + (normalize_or_zero(delta) * approach_speed);
  const Vec2 avoidance = drone_avoidance(drone, registry, desired_velocity, sector, tuning);
  if (length(avoidance) > 0.0001F) {
    desired_velocity = target_velocity + (normalize_or_zero(normalize_or_zero(delta) + avoidance) * approach_speed);
  }
  drone.velocity += clamp_length(desired_velocity - drone.velocity, acceleration * scaled_dt);
  drone.position = wrap_position(drone.position + (drone.velocity * scaled_dt), sector);
  update_facing_from_velocity(drone, tuning.facing_dead_stick_speed);
}

[[nodiscard]] bool update_cargo_haul(MiningDrone& drone, entt::registry& registry, const SectorTuning& sector,
                                     float dt_seconds, const MiningDroneTuning& tuning, DomainEventBus* event_bus,
                                     MiningDroneHudSnapshot& hud) {
  if (!valid_cargo_target(registry, drone.cargo_target)) {
    (void)transition_mining_drone_cargo(drone, MiningDroneCargoTransition::CargoDelivered);
    drone.cargo_target = entt::null;
    return false;
  }

  CargoBox& box = registry.get<CargoBox>(drone.cargo_target);
  if (box.state == CargoBoxState::PendingPickup) {
    const Vec2 to_box = wrapped_delta(drone.position, box.position, sector);
    const float distance = length(to_box);
    (void)transition_mining_drone_cargo(drone, MiningDroneCargoTransition::AssignCargo);
    hud.phase = drone.phase;
    hud.target = drone.cargo_target;
    hud.target_distance = distance;
    if (distance > tuning.cargo_pickup_tolerance) {
      move_drone_toward(drone, registry, box.position, box.velocity, sector, dt_seconds, tuning);
    } else {
      (void)transition_cargo_box(box, CargoBoxTransition::StartHaul, drone.cargo_target, event_bus);
      (void)transition_mining_drone_cargo(drone, MiningDroneCargoTransition::CargoPickedUp);
      emit_cargo_pickup_started(event_bus, entt::null, drone.cargo_target, box.position);
    }
    return true;
  }

  const Vec2 delivery_position = drone.cargo_destination;
  const Vec2 to_delivery = wrapped_delta(drone.position, delivery_position, sector);
  const float distance = length(to_delivery);
  (void)transition_mining_drone_cargo(drone, MiningDroneCargoTransition::CargoPickedUp);
  hud.phase = drone.phase;
  hud.target = drone.cargo_target;
  hud.target_distance = distance;
  if (distance > tuning.cargo_delivery_tolerance) {
    move_drone_toward(drone, registry, delivery_position, {}, sector, dt_seconds, tuning);
    box.position = drone.position;
    box.velocity = drone.velocity;
  } else {
    (void)transition_cargo_box(box, CargoBoxTransition::Link, drone.cargo_target, event_bus);
    box.velocity = {};
    emit_cargo_delivered(event_bus, entt::null, drone.cargo_target, delivery_position);
    (void)transition_mining_drone_cargo(drone, MiningDroneCargoTransition::CargoDelivered);
    drone.cargo_target = entt::null;
    drone.cargo_destination = {};
  }
  return true;
}

} // namespace

void install_drone_presence_event_handlers(entt::registry& registry, const std::vector<entt::entity>& drones,
                                           DomainEventBus& event_bus, const DronePresenceTuning& tuning) {
  auto transition = [&registry, &event_bus, &drones, tuning](const DomainEvent& event, DomainEventType request) {
    if (event.subject == entt::null || std::find(drones.begin(), drones.end(), event.subject) == drones.end())
      return;
    DronePresence& presence = registry.get<DronePresence>(event.subject);
    sml::sm<DronePresenceMachine> machine;
    replay_presence(machine, presence.phase);
    const bool accepted = request == DomainEventType::DroneExitRequested ? machine.process_event(exit_requested{})
                          : request == DomainEventType::DroneDestroyed ? machine.process_event(destroyed{})
                          : request == DomainEventType::DroneSpawnRequested
                              ? machine.process_event(spawn_requested{})
                              : machine.process_event(barrel_roll_completed{});
    if (!accepted)
      return;
    presence.phase = read_presence(machine);
    presence.phase_seconds_remaining = presence.phase == DronePresencePhase::DestroyedAwaitingRespawn
                                         ? tuning.destroyed_respawn_seconds
                                         : presence.phase == DronePresencePhase::SpawnBarrelRoll || presence.phase == DronePresencePhase::ExitBarrelRoll
                                             ? tuning.barrel_roll_seconds
                                             : 0.0F;
    presence.roll_radians = 0.0F;
    if (presence.phase == DronePresencePhase::Hidden) {
      registry.get<MiningDrone>(event.subject).velocity = {};
      event_bus.enqueue(DomainEventType::DroneDespawned,
                        DomainEvent{.type = DomainEventType::DroneDespawned, .subject = event.subject});
    }
    if (presence.phase == DronePresencePhase::DestroyedAwaitingRespawn) {
      MiningDrone& drone = registry.get<MiningDrone>(event.subject);
      drone.velocity = {};
      drone.target = entt::null;
      drone.cargo_target = entt::null;
      if (EngineTrailModel* trail = registry.try_get<EngineTrailModel>(event.subject); trail != nullptr) {
        reset_engine_trail(*trail);
      }
    } else if (presence.phase == DronePresencePhase::SpawnBarrelRoll) {
      MiningDrone& drone = registry.get<MiningDrone>(event.subject);
      drone.integrity = drone.max_integrity;
    }
  };
  event_bus.appendListener(DomainEventType::DroneExitRequested, [transition](const DomainEvent& event) {
    transition(event, DomainEventType::DroneExitRequested);
  });
  event_bus.appendListener(DomainEventType::DroneSpawnRequested, [transition](const DomainEvent& event) {
    transition(event, DomainEventType::DroneSpawnRequested);
  });
  event_bus.appendListener(DomainEventType::DroneBarrelRollCompleted, [transition](const DomainEvent& event) {
    transition(event, DomainEventType::DroneBarrelRollCompleted);
  });
  event_bus.appendListener(DomainEventType::DroneDestroyed, [transition](const DomainEvent& event) {
    transition(event, DomainEventType::DroneDestroyed);
  });
}

void update_drone_presence(entt::registry& registry, const std::vector<entt::entity>& drones, float dt_seconds,
                           DomainEventBus& event_bus, const DronePresenceTuning& tuning, Vec2 respawn_origin) {
  const float dt = std::max(0.0F, dt_seconds);
  for (const entt::entity entity : drones) {
    DronePresence& presence = registry.get<DronePresence>(entity);
    if (presence.phase != DronePresencePhase::SpawnBarrelRoll && presence.phase != DronePresencePhase::ExitBarrelRoll &&
        presence.phase != DronePresencePhase::DestroyedAwaitingRespawn)
      continue;
    presence.phase_seconds_remaining = std::max(0.0F, presence.phase_seconds_remaining - dt);
    const float duration = std::max(tuning.barrel_roll_seconds, 0.001F);
    if (presence.phase != DronePresencePhase::DestroyedAwaitingRespawn) {
      presence.roll_radians = TauRadians * (1.0F - (presence.phase_seconds_remaining / duration));
    }
    if (presence.phase_seconds_remaining <= 0.0001F) {
      if (presence.phase == DronePresencePhase::DestroyedAwaitingRespawn) {
        MiningDrone& drone = registry.get<MiningDrone>(entity);
        drone.position = respawn_origin;
      }
      const DomainEventType event_type = presence.phase == DronePresencePhase::DestroyedAwaitingRespawn
                                           ? DomainEventType::DroneSpawnRequested
                                           : DomainEventType::DroneBarrelRollCompleted;
      event_bus.enqueue(event_type, DomainEvent{.type = event_type, .subject = entity});
    }
  }
}

bool transition_mining_drone_cargo(MiningDrone& drone, MiningDroneCargoTransition transition) {
  sml::sm<DroneCargoMachine> machine;
  replay_cargo_phase(machine, drone.phase, drone.cargo_target);
  const MiningDronePhase previous = drone.phase;
  bool accepted = false;
  switch (transition) {
  case MiningDroneCargoTransition::AssignCargo:
    accepted = machine.process_event(cargo_assigned{});
    break;
  case MiningDroneCargoTransition::CargoPickedUp:
    accepted = machine.process_event(cargo_picked_up{});
    break;
  case MiningDroneCargoTransition::CargoDelivered:
    accepted = machine.process_event(cargo_delivered{});
    break;
  }
  if (!accepted) {
    return false;
  }
  drone.phase = read_cargo_phase(machine);
  return drone.phase != previous;
}

bool transition_mining_drone_work(MiningDrone& drone, MiningDroneWorkTransition transition) {
  sml::sm<DroneWorkMachine> machine;
  replay_work_phase(machine, drone.phase);
  const MiningDronePhase previous = drone.phase;
  bool accepted = false;
  switch (transition) {
  case MiningDroneWorkTransition::ReturnToFormation:
    accepted = machine.process_event(return_to_formation{});
    break;
  case MiningDroneWorkTransition::TravelToWork:
    accepted = machine.process_event(travel_to_work{});
    break;
  case MiningDroneWorkTransition::BeginMining:
    accepted = machine.process_event(begin_mining{});
    break;
  }
  if (!accepted) {
    return false;
  }
  drone.phase = read_work_phase(machine);
  return drone.phase != previous;
}

MiningDroneHudSnapshot update_mining_drone(MiningDrone& drone, entt::registry& registry,
                                           const TargetLockModel& mining_priority, const ShipMotion& ship,
                                           const SectorTuning& sector, float dt_seconds,
                                           const MiningDroneTuning& tuning, DomainEventBus* event_bus) {
  HYPERVERSE_PROFILE_ZONE("Drone navigation");
  const float scaled_dt = std::max(0.0F, dt_seconds);
  drone.work_angle_radians =
      std::fmod(drone.work_angle_radians + (tuning.work_angle_rotation_radians_per_second * scaled_dt), TauRadians);
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
    (void)transition_mining_drone_work(drone, MiningDroneWorkTransition::ReturnToFormation);
    hud.target_distance = length(to_formation);
    move_drone_toward(drone, registry, formation_position, ship.velocity, sector, dt_seconds, tuning);
    hud.phase = drone.phase;
    hud.target = drone.target;
    return hud;
  }

  AsteroidBody& asteroid = registry.get<AsteroidBody>(drone.target);
  MiningResource& resource = registry.get<MiningResource>(drone.target);
  const float work_radius = asteroid.radius + tuning.work_standoff;
  const Vec2 work_position =
      wrap_position(asteroid.position + (direction_from_angle(drone.work_angle_radians) * work_radius), sector);
  const Vec2 to_work_position = wrapped_delta(drone.position, work_position, sector);
  const Vec2 to_target = wrapped_delta(drone.position, asteroid.position, sector);
  hud.target_distance = length(to_target);

  if (length(to_work_position) > tuning.arrival_tolerance) {
    (void)transition_mining_drone_work(drone, MiningDroneWorkTransition::TravelToWork);
    move_drone_toward(drone, registry, work_position, asteroid.velocity, sector, dt_seconds, tuning);
  } else {
    (void)transition_mining_drone_work(drone, MiningDroneWorkTransition::BeginMining);
    move_drone_toward(drone, registry, work_position, asteroid.velocity, sector, dt_seconds, tuning);
    resource.integrity = std::max(0.0F, resource.integrity - (tuning.integrity_damage_per_second * scaled_dt));
    const float extracted_mass =
        extract_asteroid_mass(registry, drone.target, tuning.extraction_per_second * scaled_dt);
    record_extracted_material(resource, registry.try_get<MineralComposition>(drone.target), extracted_mass);
    drone.extracted_mass += extracted_mass;
  }

  hud.phase = drone.phase;
  hud.target = drone.target;
  hud.extracted_mass = drone.extracted_mass;
  return hud;
}

void resolve_mining_drone_overlaps(
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  const SectorTuning& sector,
  const MiningDroneTuning& tuning
) {
  HYPERVERSE_PROFILE_ZONE("Drone contact constraints");
  const float minimum_distance = std::max(0.0F, tuning.collision_radius) * 2.0F;
  for (std::size_t outer = 0; outer < drones.size(); ++outer) {
    if (!registry.valid(drones[outer]) || !registry.all_of<MiningDrone>(drones[outer])) {
      continue;
    }
    MiningDrone& first = registry.get<MiningDrone>(drones[outer]);
    if (first.integrity <= 0.0F) {
      continue;
    }
    for (std::size_t inner = outer + 1U; inner < drones.size(); ++inner) {
      if (!registry.valid(drones[inner]) || !registry.all_of<MiningDrone>(drones[inner])) {
        continue;
      }
      MiningDrone& second = registry.get<MiningDrone>(drones[inner]);
      if (second.integrity <= 0.0F) {
        continue;
      }
      Vec2 delta = wrapped_delta(first.position, second.position, sector);
      const float distance = length(delta);
      if (distance >= minimum_distance) {
        continue;
      }
      if (distance <= 0.0001F) {
        delta = {.x = 1.0F, .y = 0.0F};
      }
      const Vec2 normal = normalize_or_zero(delta);
      const float correction = (minimum_distance - distance) * 0.5F;
      first.position = wrap_position(first.position - (normal * correction), sector);
      second.position = wrap_position(second.position + (normal * correction), sector);
      const float closing_speed = dot(second.velocity - first.velocity, normal);
      if (closing_speed < 0.0F) {
        const Vec2 velocity_correction = normal * (closing_speed * 0.5F);
        first.velocity += velocity_correction;
        second.velocity -= velocity_correction;
      }
    }
  }
}

} // namespace hyperverse
