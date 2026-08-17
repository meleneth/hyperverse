#include "test_common.hpp"
#include "hyperverse/system_menu.hpp"

TEST_CASE("system menu toggles with events and confirms selected action") {
  hyperverse::DomainEventBus events;
  hyperverse::SystemMenuModel menu;
  hyperverse::install_system_menu_event_handlers(menu, events);
  int exits = 0;
  events.appendListener(hyperverse::DomainEventType::SystemMenuExitSelected, [&](const hyperverse::DomainEvent&) { ++exits; });
  events.enqueue(hyperverse::DomainEventType::SystemMenuToggleRequested, hyperverse::DomainEvent{.type = hyperverse::DomainEventType::SystemMenuToggleRequested});
  events.process();
  CHECK(menu.phase == hyperverse::SystemMenuPhase::Open);
  events.enqueue(hyperverse::DomainEventType::SystemMenuSelectionChanged, hyperverse::DomainEvent{.type = hyperverse::DomainEventType::SystemMenuSelectionChanged});
  events.enqueue(hyperverse::DomainEventType::SystemMenuConfirmed, hyperverse::DomainEvent{.type = hyperverse::DomainEventType::SystemMenuConfirmed});
  events.process();
  CHECK(menu.phase == hyperverse::SystemMenuPhase::Closed);
  CHECK(exits == 1);
  events.enqueue(hyperverse::DomainEventType::SystemMenuToggleRequested, hyperverse::DomainEvent{.type = hyperverse::DomainEventType::SystemMenuToggleRequested});
  events.process();
  CHECK(menu.phase == hyperverse::SystemMenuPhase::Open);
  events.enqueue(hyperverse::DomainEventType::SystemMenuToggleRequested, hyperverse::DomainEvent{.type = hyperverse::DomainEventType::SystemMenuToggleRequested});
  events.process();
  CHECK(menu.phase == hyperverse::SystemMenuPhase::Closed);
}
