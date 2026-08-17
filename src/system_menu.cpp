#include "hyperverse/system_menu.hpp"

#include <boost/sml.hpp>

namespace hyperverse {
namespace {
namespace sml = boost::sml;
struct menu_closed {};
struct menu_open {};
struct toggle_requested {};
struct SystemMenuMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<menu_closed> + event<toggle_requested> = state<menu_open>,
      state<menu_open> + event<toggle_requested> = state<menu_closed>
    );
  }
};
}  // namespace

void install_system_menu_event_handlers(SystemMenuModel& menu, DomainEventBus& event_bus) {
  event_bus.appendListener(DomainEventType::SystemMenuToggleRequested, [&menu](const DomainEvent&) {
    sml::sm<SystemMenuMachine> machine;
    if (menu.phase == SystemMenuPhase::Open) machine.process_event(toggle_requested{});
    if (!machine.process_event(toggle_requested{})) return;
    menu.phase = machine.is(sml::state<menu_open>) ? SystemMenuPhase::Open : SystemMenuPhase::Closed;
    if (menu.phase == SystemMenuPhase::Open) menu.selection = SystemMenuSelection::Restart;
  });
  event_bus.appendListener(DomainEventType::SystemMenuSelectionChanged, [&menu](const DomainEvent&) {
    if (menu.phase != SystemMenuPhase::Open) return;
    menu.selection = menu.selection == SystemMenuSelection::Restart ? SystemMenuSelection::Exit : SystemMenuSelection::Restart;
  });
  event_bus.appendListener(DomainEventType::SystemMenuConfirmed, [&menu, &event_bus](const DomainEvent&) {
    if (menu.phase != SystemMenuPhase::Open) return;
    const DomainEventType selected = menu.selection == SystemMenuSelection::Exit
      ? DomainEventType::SystemMenuExitSelected : DomainEventType::SystemMenuRestartSelected;
    event_bus.dispatch(selected, DomainEvent{.type = selected});
    sml::sm<SystemMenuMachine> machine;
    machine.process_event(toggle_requested{});
    machine.process_event(toggle_requested{});
    menu.phase = SystemMenuPhase::Closed;
  });
}

}  // namespace hyperverse
