#pragma once

#include "hyperverse/domain_events.hpp"
#include "hyperverse/math.hpp"

#include <entt/entity/registry.hpp>
#include <vector>

namespace hyperverse {

enum class PlayerLifecyclePhase { Alive, AwaitingRespawn };

struct PlayerLifecycleModel {
  PlayerLifecyclePhase phase{PlayerLifecyclePhase::Alive};
  Vec2 home_position{};
  float respawn_seconds_remaining{0.0F};
};

struct PlayerLifecycleTuning {
  float respawn_delay_seconds{15.0F};
};

void install_player_lifecycle_event_handlers(
  PlayerLifecycleModel& lifecycle,
  entt::registry& registry,
  entt::entity player,
  const std::vector<entt::entity>& drones,
  DomainEventBus& event_bus,
  const PlayerLifecycleTuning& tuning = {}
);

void update_player_lifecycle(
  PlayerLifecycleModel& lifecycle,
  const entt::registry& registry,
  entt::entity player,
  float dt_seconds,
  DomainEventBus& event_bus
);

}  // namespace hyperverse
