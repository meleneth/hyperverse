#include "hyperverse/cargo_escort.hpp"

#include "hyperverse/cargo_route.hpp"

#include <boost/sml.hpp>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

struct mining_phase {};
struct authorized_phase {};
struct escort_active_phase {};
struct complete_phase {};

struct quota_authorized {};
struct quota_lost {};
struct confirm_extraction {};
struct gate_reached {};

struct CargoEscortMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<mining_phase> + event<quota_authorized> = state<authorized_phase>,
      state<authorized_phase> + event<quota_lost> = state<mining_phase>,
      state<authorized_phase> + event<confirm_extraction> = state<escort_active_phase>,
      state<escort_active_phase> + event<gate_reached> = state<complete_phase>
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
    case CargoEscortPhase::Complete:
      machine.process_event(quota_authorized{});
      machine.process_event(confirm_extraction{});
      machine.process_event(gate_reached{});
      return;
  }
}

[[nodiscard]] CargoEscortPhase read_phase(const sml::sm<CargoEscortMachine>& machine) {
  if (machine.is(sml::state<complete_phase>)) {
    return CargoEscortPhase::Complete;
  }
  if (machine.is(sml::state<escort_active_phase>)) {
    return CargoEscortPhase::EscortActive;
  }
  if (machine.is(sml::state<authorized_phase>)) {
    return CargoEscortPhase::Authorized;
  }
  return CargoEscortPhase::Mining;
}

}  // namespace

CargoEscortHudSnapshot update_cargo_escort_state(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const SemanticInputFrame& input
) {
  sml::sm<CargoEscortMachine> machine;
  replay_phase(machine, escort.phase);

  if (escort.phase != CargoEscortPhase::EscortActive && escort.phase != CargoEscortPhase::Complete) {
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

  return {
    .phase = escort.phase,
    .extraction_authorized = cargo.extraction_authorized,
    .cargo_train_active = escort.phase == CargoEscortPhase::EscortActive,
  };
}

CargoEscortHudSnapshot update_cargo_escort_arrival(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const CargoEscortRouteHudSnapshot& route
) {
  sml::sm<CargoEscortMachine> machine;
  replay_phase(machine, escort.phase);
  if (route.gate_reached) {
    machine.process_event(gate_reached{});
    escort.phase = read_phase(machine);
  }

  return {
    .phase = escort.phase,
    .extraction_authorized = cargo.extraction_authorized,
    .cargo_train_active = escort.phase == CargoEscortPhase::EscortActive,
  };
}

}  // namespace hyperverse
