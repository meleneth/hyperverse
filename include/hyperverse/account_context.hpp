#pragma once

#include "hyperverse/account_state.hpp"
#include "hyperverse/domain_events.hpp"
#include "hyperverse/physics.hpp"
#include "hyperverse/scoped_log.hpp"

#include <entt/entity/registry.hpp>

#include <random>

namespace hyperverse {

class AccountCtx {
public:
  AccountCtx(
    entt::registry& registry,
    std::mt19937& rng,
    ScopedLog& log,
    AccountState& account,
    PhysicsWorld& physics,
    DomainEventBus& event_bus
  );

  [[nodiscard]] entt::registry& registry() const;
  [[nodiscard]] std::mt19937& rng() const;
  [[nodiscard]] ScopedLog& log() const;
  [[nodiscard]] AccountState& account() const;
  [[nodiscard]] PhysicsWorld& physics() const;
  [[nodiscard]] DomainEventBus& event_bus() const;

private:
  entt::registry* registry_;
  std::mt19937* rng_;
  ScopedLog* log_;
  AccountState* account_;
  PhysicsWorld* physics_;
  DomainEventBus* event_bus_;
};

}  // namespace hyperverse
