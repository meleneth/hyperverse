#pragma once

#include "hyperverse/account_context.hpp"
#include "hyperverse/domain_events.hpp"
#include "hyperverse/physics.hpp"
#include "hyperverse/scoped_log.hpp"

#include <entt/entity/registry.hpp>

#include <iosfwd>
#include <random>

namespace hyperverse {

class GrandCentral {
public:
  explicit GrandCentral(std::ostream& log_output);

  [[nodiscard]] AccountCtx account_context();

private:
  entt::registry registry_;
  std::mt19937 rng_;
  ScopedLog log_;
  AccountState account_;
  PhysicsWorld physics_;
  DomainEventBus event_bus_;
};

}  // namespace hyperverse
