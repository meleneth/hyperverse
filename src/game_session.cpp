#include "hyperverse/game_session.hpp"

#include <boost/sml.hpp>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

struct choosing_contract {};
struct playing_round {};
struct contract_accepted {};
struct round_completed {};

struct GameSessionMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<choosing_contract> + event<contract_accepted> = state<playing_round>,
      state<playing_round> + event<round_completed> = state<choosing_contract>
    );
  }
};

void replay_phase(sml::sm<GameSessionMachine>& machine, GameSessionPhase phase) {
  if (phase == GameSessionPhase::PlayingRound) {
    machine.process_event(contract_accepted{});
  }
}

[[nodiscard]] GameSessionPhase read_phase(const sml::sm<GameSessionMachine>& machine) {
  return machine.is(sml::state<playing_round>) ? GameSessionPhase::PlayingRound : GameSessionPhase::ContractChooser;
}

void process_contract_accepted(GameSessionModel& session) {
  sml::sm<GameSessionMachine> machine;
  replay_phase(machine, session.phase);
  machine.process_event(contract_accepted{});
  session.phase = read_phase(machine);
}

void process_round_completed(GameSessionModel& session) {
  sml::sm<GameSessionMachine> machine;
  replay_phase(machine, session.phase);
  machine.process_event(round_completed{});
  session.phase = read_phase(machine);
}

}  // namespace

void install_game_session_event_handlers(GameSessionModel& session, DomainEventBus& event_bus) {
  event_bus.appendListener(DomainEventType::ContractAccepted, [&session](const DomainEvent&) { process_contract_accepted(session); });
  event_bus.appendListener(DomainEventType::ContractRoundCompleted, [&session](const DomainEvent&) { process_round_completed(session); });
}

void accept_contract(GameSessionModel& session, DomainEventBus& event_bus) {
  event_bus.enqueue(DomainEventType::ContractAccepted, DomainEvent{.type = DomainEventType::ContractAccepted});
  process_contract_accepted(session);
}

void complete_contract_round(GameSessionModel& session, DomainEventBus& event_bus) {
  event_bus.enqueue(DomainEventType::ContractRoundCompleted, DomainEvent{.type = DomainEventType::ContractRoundCompleted});
  process_round_completed(session);
}

}  // namespace hyperverse
