#pragma once

#include "hyperverse/domain_events.hpp"

namespace hyperverse {

enum class GameSessionPhase {
  ContractChooser,
  PlayingRound,
};

struct GameSessionModel {
  GameSessionPhase phase{GameSessionPhase::PlayingRound};
};

void install_game_session_event_handlers(GameSessionModel& session, DomainEventBus& event_bus);
void accept_contract(GameSessionModel& session, DomainEventBus& event_bus);
void complete_contract_round(GameSessionModel& session, DomainEventBus& event_bus);

}  // namespace hyperverse
