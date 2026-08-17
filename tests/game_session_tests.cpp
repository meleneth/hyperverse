#include "test_common.hpp"

TEST_CASE("game session returns to contract chooser when round completes") {
  hyperverse::GameSessionModel session{.phase = hyperverse::GameSessionPhase::PlayingRound};
  hyperverse::DomainEventBus event_bus;
  hyperverse::install_game_session_event_handlers(session, event_bus);

  event_bus.enqueue(
    hyperverse::DomainEventType::ContractRoundCompleted,
    hyperverse::DomainEvent{.type = hyperverse::DomainEventType::ContractRoundCompleted}
  );
  event_bus.process();

  CHECK(session.phase == hyperverse::GameSessionPhase::ContractChooser);
}

TEST_CASE("game session changes only when its queued domain event is processed") {
  hyperverse::GameSessionModel session{.phase = hyperverse::GameSessionPhase::ContractChooser};
  hyperverse::DomainEventBus event_bus;
  hyperverse::install_game_session_event_handlers(session, event_bus);

  hyperverse::accept_contract(session, event_bus);
  CHECK(session.phase == hyperverse::GameSessionPhase::ContractChooser);
  event_bus.process();
  CHECK(session.phase == hyperverse::GameSessionPhase::PlayingRound);

  hyperverse::complete_contract_round(session, event_bus);
  CHECK(session.phase == hyperverse::GameSessionPhase::PlayingRound);
  event_bus.process();
  CHECK(session.phase == hyperverse::GameSessionPhase::ContractChooser);
}
