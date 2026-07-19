#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <vector>

namespace hyperverse {

struct RadarTrackedTarget {
  entt::entity target{entt::null};
  float reveal_seconds{0.0F};
  float distance{0.0F};
};

struct RadarHudModel {
  std::vector<RadarTrackedTarget> tracked_targets{};
  std::vector<entt::entity> target_order{};
  float update_seconds_remaining{0.0F};
};

struct CombatRadarHudModel {
  std::vector<RadarTrackedTarget> tracked_targets{};
  std::vector<entt::entity> target_order{};
  float update_seconds_remaining{0.0F};
};

struct RadarHudTuning {
  int max_targets{10};
  float update_interval_seconds{0.25F};
  float reveal_seconds{0.5F};
  float range_world{0.0F};
};

void update_radar_hud(
  RadarHudModel& radar,
  entt::registry& registry,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RadarHudTuning& tuning
);

void update_combat_radar_hud(
  CombatRadarHudModel& radar,
  entt::registry& registry,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RadarHudTuning& tuning
);

}  // namespace hyperverse
