#include "hyperverse/game_context.hpp"

namespace hyperverse {

SectorTickCtx::SectorTickCtx(AccountCtx& account, const SectorTuning& sector, float dt_seconds)
    : account_{&account}, sector_{&sector}, dt_seconds_{dt_seconds} {}

entt::registry& SectorTickCtx::registry() const {
  return account_->registry();
}

std::mt19937& SectorTickCtx::rng() const {
  return account_->rng();
}

ScopedLog& SectorTickCtx::log() const {
  return account_->log();
}

DomainEventBus& SectorTickCtx::event_bus() const {
  return account_->event_bus();
}

const SectorTuning& SectorTickCtx::sector() const {
  return *sector_;
}

float SectorTickCtx::dt() const {
  return dt_seconds_;
}

EntityCtx SectorTickCtx::entity_context(entt::entity self) const {
  return {*this, self};
}

EntityCtx::EntityCtx(SectorTickCtx tick, entt::entity self) : tick_{tick}, self_{self} {}

entt::registry& EntityCtx::registry() const {
  return tick_.registry();
}

std::mt19937& EntityCtx::rng() const {
  return tick_.rng();
}

ScopedLog& EntityCtx::log() const {
  return tick_.log();
}

DomainEventBus& EntityCtx::event_bus() const {
  return tick_.event_bus();
}

const SectorTuning& EntityCtx::sector() const {
  return tick_.sector();
}

float EntityCtx::dt() const {
  return tick_.dt();
}

entt::entity EntityCtx::self() const {
  return self_;
}

EntityCtx EntityCtx::entity_context(entt::entity self) const {
  return tick_.entity_context(self);
}

}  // namespace hyperverse
