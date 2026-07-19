#pragma once

#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/domain_events.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class CargoExtractionPhase {
  Idle,
  Queueing,
  MovingActiveToGate,
  ExtractingActive,
  Complete,
};

enum class CargoExtractionTransition {
  BeginQueue,
  ActiveNeedsGate,
  ActiveAtGate,
  ActiveExtracted,
  QueueEmpty,
  Reset,
};

struct CargoExtractionModel {
  CargoExtractionPhase phase{CargoExtractionPhase::Idle};
  entt::entity active_box{entt::null};
};

struct CargoExtractionTuning {
  float seconds_per_box{5.0F};
  float gate_radius{96.0F};
  float staging_radius{140.0F};
  float staging_spacing{156.0F};
  int formation_columns{5};
  float approach_rate{4.0F};
  float max_speed{520.0F};
};

struct CargoExtractionHudSnapshot {
  int extracted_boxes{0};
  int total_boxes{0};
  int active_box_index{-1};
  float active_box_fraction{0.0F};
  bool active{false};
  bool complete{false};
};

[[nodiscard]] CargoExtractionHudSnapshot update_cargo_extraction(
  CargoExtractionModel& model,
  entt::registry& registry,
  CargoEscortState& escort,
  const CargoEscortRoute& route,
  const SectorTuning& sector,
  float dt_seconds,
  DomainEventBus* event_bus = nullptr,
  const CargoExtractionTuning& tuning = {}
);

}  // namespace hyperverse
