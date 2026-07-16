#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class HarpoonPhase {
  Ready,
  Latched,
};

struct HarpoonModel {
  HarpoonPhase phase{HarpoonPhase::Ready};
  entt::entity target{entt::null};
};

struct HarpoonTuning {
  float latch_range{1600.0F};
  float release_range{2400.0F};
  float asteroid_brake_per_second{2400.0F};
  float ship_pull_per_second{720.0F};
  float ship_effective_mass{120.0F};
};

struct HarpoonHudSnapshot {
  bool latched{false};
  entt::entity target{entt::null};
  float target_distance{0.0F};
  float target_speed{0.0F};
};

[[nodiscard]] HarpoonHudSnapshot update_harpoon(
  HarpoonModel& model,
  entt::registry& registry,
  const TargetLockModel& target_lock,
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const HarpoonTuning& tuning = {}
);

}  // namespace hyperverse
