#include "hyperverse/cargo_escort.hpp"

#include "hyperverse/cargo_route.hpp"

#include <boost/sml.hpp>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

struct mining_phase {};
struct authorized_phase {};
struct escort_active_phase {};
struct extracting_phase {};
struct complete_phase {};

struct quota_authorized {};
struct quota_lost {};
struct confirm_extraction {};
struct gate_reached {};
struct extraction_complete {};

struct CargoEscortMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<mining_phase> + event<quota_authorized> = state<authorized_phase>,
      state<authorized_phase> + event<quota_lost> = state<mining_phase>,
      state<authorized_phase> + event<confirm_extraction> = state<escort_active_phase>,
      state<escort_active_phase> + event<gate_reached> = state<extracting_phase>,
      state<extracting_phase> + event<extraction_complete> = state<complete_phase>
    );
  }
};

void replay_phase(sml::sm<CargoEscortMachine>& machine, CargoEscortPhase phase) {
  switch (phase) {
    case CargoEscortPhase::Mining:
      return;
    case CargoEscortPhase::Authorized:
      machine.process_event(quota_authorized{});
      return;
    case CargoEscortPhase::EscortActive:
      machine.process_event(quota_authorized{});
      machine.process_event(confirm_extraction{});
      return;
    case CargoEscortPhase::Extracting:
      machine.process_event(quota_authorized{});
      machine.process_event(confirm_extraction{});
      machine.process_event(gate_reached{});
      return;
    case CargoEscortPhase::Complete:
      machine.process_event(quota_authorized{});
      machine.process_event(confirm_extraction{});
      machine.process_event(gate_reached{});
      machine.process_event(extraction_complete{});
      return;
  }
}

[[nodiscard]] CargoEscortPhase read_phase(const sml::sm<CargoEscortMachine>& machine) {
  if (machine.is(sml::state<complete_phase>)) {
    return CargoEscortPhase::Complete;
  }
  if (machine.is(sml::state<extracting_phase>)) {
    return CargoEscortPhase::Extracting;
  }
  if (machine.is(sml::state<escort_active_phase>)) {
    return CargoEscortPhase::EscortActive;
  }
  if (machine.is(sml::state<authorized_phase>)) {
    return CargoEscortPhase::Authorized;
  }
  return CargoEscortPhase::Mining;
}

void emit_escort_started(DomainEventBus* event_bus) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(DomainEventType::CargoEscortStarted, DomainEvent{.type = DomainEventType::CargoEscortStarted});
}

void emit_arrived_at_gate(DomainEventBus* event_bus, const CargoEscortRouteHudSnapshot& route) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoArrivedAtGate,
    DomainEvent{
      .type = DomainEventType::CargoArrivedAtGate,
      .position = route.gate_position,
    }
  );
}

}  // namespace

bool transition_cargo_escort(CargoEscortState& escort, CargoEscortTransition transition) {
  sml::sm<CargoEscortMachine> machine;
  replay_phase(machine, escort.phase);
  const CargoEscortPhase previous = escort.phase;
  bool accepted = false;
  switch (transition) {
    case CargoEscortTransition::QuotaAuthorized:
      accepted = machine.process_event(quota_authorized{});
      break;
    case CargoEscortTransition::QuotaLost:
      accepted = machine.process_event(quota_lost{});
      break;
    case CargoEscortTransition::ConfirmExtraction:
      accepted = machine.process_event(confirm_extraction{});
      break;
    case CargoEscortTransition::GateReached:
      accepted = machine.process_event(gate_reached{});
      break;
    case CargoEscortTransition::ExtractionComplete:
      accepted = machine.process_event(extraction_complete{});
      break;
  }
  if (!accepted) {
    return false;
  }
  escort.phase = read_phase(machine);
  return escort.phase != previous;
}

CargoEscortHudSnapshot update_cargo_escort_state(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const SemanticInputFrame& input,
  DomainEventBus* event_bus
) {
  sml::sm<CargoEscortMachine> machine;
  replay_phase(machine, escort.phase);
  const CargoEscortPhase previous_phase = escort.phase;

  if (
    escort.phase != CargoEscortPhase::EscortActive && escort.phase != CargoEscortPhase::Extracting &&
    escort.phase != CargoEscortPhase::Complete
  ) {
    if (cargo.extraction_authorized) {
      machine.process_event(quota_authorized{});
    } else {
      machine.process_event(quota_lost{});
    }
    if (cargo.extraction_authorized && input.confirm_requested) {
      machine.process_event(confirm_extraction{});
    }
    escort.phase = read_phase(machine);
  }
  if (previous_phase != CargoEscortPhase::EscortActive && escort.phase == CargoEscortPhase::EscortActive) {
    emit_escort_started(event_bus);
  }

  return {
    .phase = escort.phase,
    .extraction_authorized = cargo.extraction_authorized,
    .cargo_train_active = escort.phase == CargoEscortPhase::EscortActive,
    .cargo_extracting = escort.phase == CargoEscortPhase::Extracting,
  };
}

CargoEscortHudSnapshot update_cargo_escort_arrival(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const CargoEscortRouteHudSnapshot& route,
  DomainEventBus* event_bus
) {
  sml::sm<CargoEscortMachine> machine;
  replay_phase(machine, escort.phase);
  const CargoEscortPhase previous_phase = escort.phase;
  if (route.gate_reached) {
    machine.process_event(gate_reached{});
    escort.phase = read_phase(machine);
  }
  if (previous_phase == CargoEscortPhase::EscortActive && escort.phase == CargoEscortPhase::Extracting) {
    emit_arrived_at_gate(event_bus, route);
  }

  return {
    .phase = escort.phase,
    .extraction_authorized = cargo.extraction_authorized,
    .cargo_train_active = escort.phase == CargoEscortPhase::EscortActive,
    .cargo_extracting = escort.phase == CargoEscortPhase::Extracting,
  };
}

}  // namespace hyperverse
