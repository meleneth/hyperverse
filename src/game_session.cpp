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

}  // namespace

struct GameSessionFsm::Impl {
  sml::sm<GameSessionMachine> machine;
};

GameSessionFsm::GameSessionFsm() : impl_{std::make_shared<Impl>()} {}

void GameSessionFsm::initialize(GameSessionPhase phase) {
  impl_ = std::make_shared<Impl>();
  if (phase == GameSessionPhase::PlayingRound) {
    (void)impl_->machine.process_event(contract_accepted{});
  }
}

bool GameSessionFsm::accept_contract() {
  return impl_->machine.process_event(contract_accepted{});
}

bool GameSessionFsm::complete_round() {
  return impl_->machine.process_event(round_completed{});
}

GameSessionPhase GameSessionFsm::phase() const {
  return impl_->machine.is(sml::state<playing_round>) ? GameSessionPhase::PlayingRound : GameSessionPhase::ContractChooser;
}

void install_game_session_event_handlers(GameSessionModel& session, DomainEventBus& event_bus) {
  session.fsm.initialize(session.phase);
  event_bus.appendListener(DomainEventType::ContractAccepted, [&session](const DomainEvent&) {
    if (session.fsm.accept_contract()) session.phase = session.fsm.phase();
  });
  event_bus.appendListener(DomainEventType::ContractRoundCompleted, [&session](const DomainEvent&) {
    if (session.fsm.complete_round()) session.phase = session.fsm.phase();
  });
}

void accept_contract(GameSessionModel& session, DomainEventBus& event_bus) {
  (void)session;
  event_bus.enqueue(DomainEventType::ContractAccepted, DomainEvent{.type = DomainEventType::ContractAccepted});
}

void complete_contract_round(GameSessionModel& session, DomainEventBus& event_bus) {
  (void)session;
  event_bus.enqueue(DomainEventType::ContractRoundCompleted, DomainEvent{.type = DomainEventType::ContractRoundCompleted});
}

}  // namespace hyperverse
