#pragma once

#include "hyperverse/math.hpp"

#include <entt/entity/entity.hpp>
#include <eventpp/eventqueue.h>

namespace hyperverse {

enum class DomainEventType {
  ParticleFired,
  ParticleImpact,
  AsteroidDamaged,
  AsteroidFragmented,
  AsteroidConsumed,
  DroneTargetReleased,
  ContractAccepted,
  CargoEscortStarted,
  CargoArrivedAtGate,
  CargoBoxExtracted,
  CargoExtractionComplete,
  ContractRoundCompleted,
};

struct DomainEvent {
  DomainEventType type{DomainEventType::ParticleFired};
  entt::entity subject{entt::null};
  entt::entity target{entt::null};
  Vec2 position{};
  float amount{0.0F};
  int count{0};
};

using DomainEventBus = eventpp::EventQueue<DomainEventType, void(const DomainEvent&)>;

}  // namespace hyperverse
