#pragma once

#include "hyperverse/input.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

struct CargoManifest {
  float delivered_mass{0.0F};
  int cargo_boxes{0};
};

struct ExtractionSite {
  Vec2 position{};
};

struct CargoBox {
  Vec2 position{};
  Vec2 velocity{};
  float mass{0.0F};
  int index{0};
};

struct CargoBoxTuning {
  float box_mass{10.0F};
  float box_spacing{78.0F};
};

struct ContractQuotaTuning {
  float required_mass{40.0F};
  float cargo_box_mass{10.0F};
  float over_quota_bonus_step_mass{20.0F};
  float bonus_per_step{0.15F};
};

struct CargoHudSnapshot {
  float delivered_mass{0.0F};
  float required_mass{0.0F};
  float quota_fraction{0.0F};
  float over_quota_mass{0.0F};
  float payout_multiplier{1.0F};
  int cargo_boxes{0};
  bool extraction_authorized{false};
};

enum class CargoEscortPhase {
  Mining,
  Authorized,
  EscortActive,
  Complete,
};

struct CargoEscortState {
  CargoEscortPhase phase{CargoEscortPhase::Mining};
};

struct CargoEscortHudSnapshot {
  CargoEscortPhase phase{CargoEscortPhase::Mining};
  bool extraction_authorized{false};
  bool cargo_train_active{false};
};

struct CargoTrainTuning {
  float link_spacing{86.0F};
  float follow_rate{5.0F};
  float max_speed{520.0F};
};

struct CargoTrainHudSnapshot {
  int linked_boxes{0};
  float train_length{0.0F};
  float max_coupling_stress{0.0F};
  bool active{false};
};

struct CargoEscortRoute {
  Vec2 gate_position{};
  float gate_radius{160.0F};
};

struct CargoEscortRouteHudSnapshot {
  Vec2 gate_position{};
  float gate_distance{0.0F};
  bool active{false};
  bool gate_reached{false};
};

[[nodiscard]] CargoHudSnapshot update_cargo_manifest(
  CargoManifest& manifest,
  entt::registry& registry,
  const ContractQuotaTuning& tuning = {}
);

int sync_cargo_boxes(
  entt::registry& registry,
  const CargoManifest& manifest,
  const ExtractionSite& extraction_site,
  const CargoBoxTuning& tuning = {}
);

[[nodiscard]] CargoEscortHudSnapshot update_cargo_escort_state(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const SemanticInputFrame& input
);

[[nodiscard]] CargoTrainHudSnapshot update_cargo_train(
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const CargoTrainTuning& tuning = {}
);

[[nodiscard]] CargoEscortRouteHudSnapshot update_cargo_escort_route(
  const CargoEscortState& escort,
  const CargoEscortRoute& route,
  const ShipMotion& ship,
  const SectorTuning& sector
);

[[nodiscard]] CargoEscortHudSnapshot update_cargo_escort_arrival(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const CargoEscortRouteHudSnapshot& route
);

}  // namespace hyperverse
