#pragma once

#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/domain_events.hpp"
#include "hyperverse/input.hpp"

namespace hyperverse {

enum class CargoEscortPhase {
  Mining,
  Authorized,
  EscortActive,
  Extracting,
  Complete,
};

struct CargoEscortState {
  CargoEscortPhase phase{CargoEscortPhase::Mining};
};

struct CargoEscortHudSnapshot {
  CargoEscortPhase phase{CargoEscortPhase::Mining};
  bool extraction_authorized{false};
  bool cargo_train_active{false};
  bool cargo_extracting{false};
};

struct CargoEscortRouteHudSnapshot;

[[nodiscard]] CargoEscortHudSnapshot update_cargo_escort_state(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const SemanticInputFrame& input,
  DomainEventBus* event_bus = nullptr
);

[[nodiscard]] CargoEscortHudSnapshot update_cargo_escort_arrival(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const CargoEscortRouteHudSnapshot& route,
  DomainEventBus* event_bus = nullptr
);

}  // namespace hyperverse
