#include "hyperverse/account_context.hpp"

namespace hyperverse {

AccountCtx::AccountCtx(
  entt::registry& registry,
  std::mt19937& rng,
  ScopedLog& log,
  AccountState& account,
  PhysicsWorld& physics,
  DomainEventBus& event_bus
)
    : registry_{&registry}, rng_{&rng}, log_{&log}, account_{&account}, physics_{&physics}, event_bus_{&event_bus} {}

entt::registry& AccountCtx::registry() const {
  return *registry_;
}

std::mt19937& AccountCtx::rng() const {
  return *rng_;
}

ScopedLog& AccountCtx::log() const {
  return *log_;
}

AccountState& AccountCtx::account() const {
  return *account_;
}

PhysicsWorld& AccountCtx::physics() const {
  return *physics_;
}

DomainEventBus& AccountCtx::event_bus() const {
  return *event_bus_;
}

}  // namespace hyperverse
