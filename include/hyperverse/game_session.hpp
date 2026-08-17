#pragma once

#include "hyperverse/domain_events.hpp"

#include <memory>

namespace hyperverse {

enum class GameSessionPhase {
  ContractChooser,
  PlayingRound,
};

class GameSessionFsm {
public:
  GameSessionFsm();
  void initialize(GameSessionPhase phase);
  [[nodiscard]] bool accept_contract();
  [[nodiscard]] bool complete_round();
  [[nodiscard]] GameSessionPhase phase() const;

private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

struct GameSessionModel {
  GameSessionPhase phase{GameSessionPhase::PlayingRound};
  GameSessionFsm fsm{};
};

void install_game_session_event_handlers(GameSessionModel& session, DomainEventBus& event_bus);
void accept_contract(GameSessionModel& session, DomainEventBus& event_bus);
void complete_contract_round(GameSessionModel& session, DomainEventBus& event_bus);

}  // namespace hyperverse
