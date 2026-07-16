#pragma once

#include "hyperverse/domain_events.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class MiningDronePhase {
  Idle,
  Travelling,
  Mining,
};

struct MiningDrone {
  Vec2 position{};
  Vec2 velocity{};
  entt::entity target{entt::null};
  MiningDronePhase phase{MiningDronePhase::Idle};
  float facing_radians{0.0F};
  float work_angle_radians{0.0F};
  float extracted_mass{0.0F};
};

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
