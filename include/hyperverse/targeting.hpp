#pragma once

#include "hyperverse/input.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

struct AsteroidBody {
  Vec2 position{};
  float radius{160.0F};
  float scan_confidence{0.25F};
};

enum class TargetLockPhase {
  Unlocked,
  Locked,
};

struct TargetingTuning {
  float lock_range{2500.0F};
  float release_range{3200.0F};
};

struct TargetLockModel {
  TargetLockPhase phase{TargetLockPhase::Unlocked};
  entt::entity target{entt::null};
  Vec2 relative_position{};
  float wrapped_distance{0.0F};
  float scan_confidence{0.0F};
};

[[nodiscard]] bool has_locked_target(const TargetLockModel& lock);

void update_target_lock(
  TargetLockModel& lock,
  entt::registry& registry,
  Vec2 observer_position,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const TargetingTuning& tuning = {}
);

}  // namespace hyperverse
