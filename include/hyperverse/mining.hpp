#pragma once

#include "hyperverse/input.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

struct MiningResource {
  float integrity{100.0F};
  float heat{0.0F};
  float extracted_mass{0.0F};
};

struct MiningLaserTuning {
  float range{1250.0F};
  float integrity_damage_per_second{18.0F};
  float extraction_per_second{8.0F};
  float heat_per_second{26.0F};
  float heat_decay_per_second{10.0F};
};

struct MiningHudSnapshot {
  bool beam_active{false};
  bool target_in_range{false};
  float beam_intensity{0.0F};
  float target_integrity{0.0F};
  float target_heat{0.0F};
  float extracted_mass{0.0F};
};

[[nodiscard]] MiningHudSnapshot update_mining_laser(
  entt::registry& registry,
  const TargetLockModel& target_lock,
  const SemanticInputFrame& input,
  const MiningLaserTuning& tuning,
  float dt_seconds
);

}  // namespace hyperverse
