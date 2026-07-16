#pragma once

#include "hyperverse/cargo.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class RaiderPhase {
  Idle,
  Approaching,
  Disrupting,
  Towing,
};

struct RaiderShip {
  Vec2 position{};
  Vec2 velocity{};
  entt::entity target_box{entt::null};
  RaiderPhase phase{RaiderPhase::Idle};
  float disruption_seconds{0.0F};
};

struct RaiderTuning {
  float max_speed{460.0F};
  float disruption_range{70.0F};
  float disruption_seconds{0.5F};
};

struct RaiderHudSnapshot {
  entt::entity target_box{entt::null};
  RaiderPhase phase{RaiderPhase::Idle};
  float target_distance{0.0F};
  float disruption_fraction{0.0F};
  float escape_distance{0.0F};
  bool active{false};
};

[[nodiscard]] RaiderHudSnapshot update_raider_threat(
  RaiderShip& raider,
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning = {}
);

}  // namespace hyperverse
