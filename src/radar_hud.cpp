#include "hyperverse/radar_hud.hpp"

#include "hyperverse/raider.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace hyperverse {
namespace {

[[nodiscard]] float existing_reveal_seconds(const RadarHudModel& radar, entt::entity target) {
  for (const RadarTrackedTarget& tracked : radar.tracked_targets) {
    if (tracked.target == target) {
      return tracked.reveal_seconds;
    }
  }
  return 0.0F;
}

[[nodiscard]] float existing_combat_reveal_seconds(const CombatRadarHudModel& radar, entt::entity target) {
  for (const RadarTrackedTarget& tracked : radar.tracked_targets) {
    if (tracked.target == target) {
      return tracked.reveal_seconds;
    }
  }
  return 0.0F;
}

}  // namespace

void update_radar_hud(
  RadarHudModel& radar,
  entt::registry& registry,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RadarHudTuning& tuning
) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const float reveal_limit = std::max(tuning.reveal_seconds, std::numeric_limits<float>::epsilon());
  for (RadarTrackedTarget& tracked : radar.tracked_targets) {
    tracked.reveal_seconds = std::min(reveal_limit, tracked.reveal_seconds + scaled_dt);
  }

  radar.update_seconds_remaining -= scaled_dt;
  if (radar.update_seconds_remaining > std::numeric_limits<float>::epsilon()) {
    return;
  }
  radar.update_seconds_remaining = std::max(tuning.update_interval_seconds, std::numeric_limits<float>::epsilon());

  std::vector<RadarTrackedTarget> candidates;
  const float range = std::max(0.0F, tuning.range_world);
  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    const float distance = wrapped_distance(ship.position, asteroid.position, sector);
    if (range > 0.0F && distance > range) {
      continue;
    }
    candidates.push_back({
      .target = entity,
      .reveal_seconds = existing_reveal_seconds(radar, entity),
      .distance = distance,
    });
  }

  std::ranges::sort(candidates, [](const RadarTrackedTarget& lhs, const RadarTrackedTarget& rhs) {
    return lhs.distance < rhs.distance;
  });
  const std::size_t keep_count = static_cast<std::size_t>(std::max(0, tuning.max_targets));
  if (candidates.size() > keep_count) {
    candidates.resize(keep_count);
  }

  radar.target_order.clear();
  radar.target_order.reserve(candidates.size());
  for (const RadarTrackedTarget& candidate : candidates) {
    radar.target_order.push_back(candidate.target);
  }
  radar.tracked_targets = std::move(candidates);
}

void update_combat_radar_hud(
  CombatRadarHudModel& radar,
  entt::registry& registry,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RadarHudTuning& tuning
) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const float reveal_limit = std::max(tuning.reveal_seconds, std::numeric_limits<float>::epsilon());
  for (RadarTrackedTarget& tracked : radar.tracked_targets) {
    tracked.reveal_seconds = std::min(reveal_limit, tracked.reveal_seconds + scaled_dt);
  }

  radar.update_seconds_remaining -= scaled_dt;
  if (radar.update_seconds_remaining > std::numeric_limits<float>::epsilon()) {
    return;
  }
  radar.update_seconds_remaining = std::max(tuning.update_interval_seconds, std::numeric_limits<float>::epsilon());

  std::vector<RadarTrackedTarget> candidates;
  const float range = std::max(0.0F, tuning.range_world);
  for (auto [entity, raider] : registry.view<RaiderShip>().each()) {
    if (
      raider.integrity <= 0.0F || raider.phase == RaiderPhase::Escaped ||
      (raider.role == RaiderRole::CargoThief && raider.phase == RaiderPhase::Idle)
    ) {
      continue;
    }
    const float distance = wrapped_distance(ship.position, raider.position, sector);
    if (range > 0.0F && distance > range) {
      continue;
    }
    candidates.push_back({
      .target = entity,
      .reveal_seconds = existing_combat_reveal_seconds(radar, entity),
      .distance = distance,
    });
  }

  std::ranges::sort(candidates, [](const RadarTrackedTarget& lhs, const RadarTrackedTarget& rhs) {
    return lhs.distance < rhs.distance;
  });
  const std::size_t keep_count = static_cast<std::size_t>(std::max(0, tuning.max_targets));
  if (candidates.size() > keep_count) {
    candidates.resize(keep_count);
  }

  radar.target_order.clear();
  radar.target_order.reserve(candidates.size());
  for (const RadarTrackedTarget& candidate : candidates) {
    radar.target_order.push_back(candidate.target);
  }
  radar.tracked_targets = std::move(candidates);
}

}  // namespace hyperverse
