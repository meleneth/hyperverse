#pragma once

#include "hyperverse/domain_events.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

namespace hyperverse {

enum class MiningDronePhase {
  Idle,
  Travelling,
  Mining,
  CargoPickup,
  EscortingCargo,
};

enum class DronePresencePhase {
  SpawnBarrelRoll,
  Following,
  ExitBarrelRoll,
  Hidden,
  DestroyedAwaitingRespawn,
};

struct DronePresence {
  DronePresencePhase phase{DronePresencePhase::SpawnBarrelRoll};
  float phase_seconds_remaining{0.0F};
  float roll_radians{0.0F};
};

struct DronePresenceTuning {
  float barrel_roll_seconds{0.8F};
  float destroyed_respawn_seconds{15.0F};
};

void install_drone_presence_event_handlers(
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  DomainEventBus& event_bus,
  const DronePresenceTuning& tuning = {}
);

void update_drone_presence(
  entt::registry& registry,
  const std::vector<entt::entity>& drones,
  float dt_seconds,
  DomainEventBus& event_bus,
  const DronePresenceTuning& tuning = {},
  Vec2 respawn_origin = {}
);

enum class MiningDroneWorkTransition {
  ReturnToFormation,
  TravelToWork,
  BeginMining,
};

enum class MiningDroneCargoTransition {
  AssignCargo,
  CargoPickedUp,
  CargoDelivered,
};

struct MiningDrone {
  Vec2 position{};
  Vec2 velocity{};
  Vec2 cargo_destination{};
  entt::entity target{entt::null};
  entt::entity cargo_target{entt::null};
  MiningDronePhase phase{MiningDronePhase::Idle};
  float facing_radians{0.0F};
  float work_angle_radians{0.0F};
  float extracted_mass{0.0F};
  float integrity{60.0F};
  float max_integrity{60.0F};
};

[[nodiscard]] bool transition_mining_drone_work(
  MiningDrone& drone,
  MiningDroneWorkTransition transition
);

[[nodiscard]] bool transition_mining_drone_cargo(
  MiningDrone& drone,
  MiningDroneCargoTransition transition
);

struct MiningDroneTuning {
  float max_speed{760.0F};
  float mining_range{220.0F};
  float work_standoff{320.0F};
  float formation_trail_distance{280.0F};
  float formation_spread{190.0F};
  float arrival_tolerance{36.0F};
  float integrity_damage_per_second{5.0F};
  float extraction_per_second{3.5F};
  float work_angle_rotation_radians_per_second{0.18F};
  float facing_dead_stick_speed{24.0F};
  float max_target_distance_from_ship{2200.0F};
  float cargo_pickup_tolerance{34.0F};
  float cargo_delivery_tolerance{42.0F};
};

struct MiningDroneHudSnapshot {
  MiningDronePhase phase{MiningDronePhase::Idle};
  entt::entity target{entt::null};
  float target_distance{0.0F};
  float extracted_mass{0.0F};
};

[[nodiscard]] MiningDroneHudSnapshot update_mining_drone(
  MiningDrone& drone,
  entt::registry& registry,
  const TargetLockModel& mining_priority,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const MiningDroneTuning& tuning = {},
  DomainEventBus* event_bus = nullptr
);

}  // namespace hyperverse
