#include "hyperverse/player_lifecycle.hpp"

#include "hyperverse/drone.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/ship_status.hpp"

#include <boost/sml.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace hyperverse {
namespace {
namespace sml = boost::sml;
struct alive {};
struct awaiting_respawn {};
struct player_died {};
struct respawn_timeout_elapsed {};
struct restart_requested {};

struct PlayerLifecycleMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<alive> + event<player_died> = state<awaiting_respawn>,
      state<awaiting_respawn> + event<respawn_timeout_elapsed> = state<alive>,
      state<alive> + event<restart_requested> = state<alive>
    );
  }
};

void enqueue_for_drones(DomainEventBus& bus, DomainEventType type, const std::vector<entt::entity>& drones) {
  for (const entt::entity drone : drones) {
    bus.dispatch(type, DomainEvent{.type = type, .subject = drone});
  }
}
}  // namespace

void install_player_lifecycle_event_handlers(
  PlayerLifecycleModel& lifecycle,
  entt::registry& registry,
  entt::entity player,
  const std::vector<entt::entity>& drones,
  DomainEventBus& event_bus,
  const PlayerLifecycleTuning& tuning
) {
  event_bus.appendListener(DomainEventType::PlayerDied, [&lifecycle, &event_bus, &drones, player, tuning](const DomainEvent& event) {
    if (event.subject != player || lifecycle.phase != PlayerLifecyclePhase::Alive) return;
    sml::sm<PlayerLifecycleMachine> machine;
    if (!machine.process_event(player_died{})) return;
    lifecycle.phase = PlayerLifecyclePhase::AwaitingRespawn;
    lifecycle.respawn_seconds_remaining = tuning.respawn_delay_seconds;
    enqueue_for_drones(event_bus, DomainEventType::DroneExitRequested, drones);
  });
  event_bus.appendListener(DomainEventType::PlayerRespawnTimeoutElapsed, [&lifecycle, &registry, &event_bus, &drones, player](const DomainEvent& event) {
    if (event.subject != player || lifecycle.phase != PlayerLifecyclePhase::AwaitingRespawn) return;
    sml::sm<PlayerLifecycleMachine> machine;
    machine.process_event(player_died{});
    if (!machine.process_event(respawn_timeout_elapsed{})) return;
    lifecycle.phase = PlayerLifecyclePhase::Alive;
    lifecycle.respawn_seconds_remaining = 0.0F;
    ShipMotion& ship = registry.get<ShipMotion>(player);
    ship.position = lifecycle.home_position;
    ship.velocity = {};
    ShipHealth& health = registry.get<ShipHealth>(player);
    health.armor = health.max_armor;
    health.shields = health.max_shields;
    event_bus.dispatch(DomainEventType::PlayerRespawned, DomainEvent{.type = DomainEventType::PlayerRespawned, .subject = player, .position = ship.position});
  });
  event_bus.appendListener(DomainEventType::PlayerRespawned, [&event_bus, &registry, &drones, player](const DomainEvent& event) {
    if (event.subject != player) return;
    for (std::size_t index = 0; index < drones.size(); ++index) {
      const float angle = (static_cast<float>(index) / static_cast<float>(std::max<std::size_t>(drones.size(), 1U))) * std::numbers::pi_v<float> * 2.0F;
      MiningDrone& drone = registry.get<MiningDrone>(drones[index]);
      drone.position = event.position + Vec2{.x = std::cos(angle) * 180.0F, .y = std::sin(angle) * 180.0F};
      drone.velocity = {};
      drone.target = entt::null;
      drone.cargo_target = entt::null;
    }
    enqueue_for_drones(event_bus, DomainEventType::DroneSpawnRequested, drones);
  });
  event_bus.appendListener(DomainEventType::PlayerRestartRequested, [&lifecycle, &registry, &event_bus, player](const DomainEvent& event) {
    if (event.subject != player || lifecycle.phase != PlayerLifecyclePhase::Alive) return;
    sml::sm<PlayerLifecycleMachine> machine;
    if (!machine.process_event(restart_requested{})) return;
    ShipMotion& ship = registry.get<ShipMotion>(player);
    ship.position = lifecycle.home_position;
    ship.velocity = {};
    ShipHealth& health = registry.get<ShipHealth>(player);
    health.armor = health.max_armor;
    health.shields = health.max_shields;
    event_bus.dispatch(DomainEventType::PlayerRespawned, DomainEvent{.type = DomainEventType::PlayerRespawned, .subject = player, .position = ship.position});
  });
}

void update_player_lifecycle(PlayerLifecycleModel& lifecycle, const entt::registry& registry, entt::entity player, float dt_seconds, DomainEventBus& event_bus) {
  if (lifecycle.phase == PlayerLifecyclePhase::Alive) {
    if (registry.get<ShipHealth>(player).armor <= 0.0F) {
      event_bus.enqueue(DomainEventType::PlayerDied, DomainEvent{.type = DomainEventType::PlayerDied, .subject = player, .position = registry.get<ShipMotion>(player).position});
    }
    return;
  }
  lifecycle.respawn_seconds_remaining = std::max(0.0F, lifecycle.respawn_seconds_remaining - std::max(0.0F, dt_seconds));
  if (lifecycle.respawn_seconds_remaining <= 0.0001F) {
    event_bus.enqueue(DomainEventType::PlayerRespawnTimeoutElapsed, DomainEvent{.type = DomainEventType::PlayerRespawnTimeoutElapsed, .subject = player});
  }
}

}  // namespace hyperverse
