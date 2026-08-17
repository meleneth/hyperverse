#pragma once

#include "hyperverse/domain_events.hpp"

namespace hyperverse {

enum class SystemMenuPhase { Closed, Open };
enum class SystemMenuSelection { Restart, Exit };

struct SystemMenuModel {
  SystemMenuPhase phase{SystemMenuPhase::Closed};
  SystemMenuSelection selection{SystemMenuSelection::Restart};
};

void install_system_menu_event_handlers(SystemMenuModel& menu, DomainEventBus& event_bus);

}  // namespace hyperverse
