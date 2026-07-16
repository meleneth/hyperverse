#pragma once

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
  float extracted_mass{0.0F};
};

struct MiningDroneTuning {
  float max_speed{520.0F};
  float mining_range{220.0F};
  float integrity_damage_per_second{5.0F};
  float extraction_per_second{3.5F};
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
  const SectorTuning& sector,
  float dt_seconds,
  const MiningDroneTuning& tuning = {}
);

}  // namespace hyperverse
