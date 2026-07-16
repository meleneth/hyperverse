#pragma once

#include "hyperverse/account_context.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <random>

namespace hyperverse {

class EntityCtx;

class SectorTickCtx {
public:
  SectorTickCtx(AccountCtx& account, const SectorTuning& sector, float dt_seconds);

  [[nodiscard]] entt::registry& registry() const;
  [[nodiscard]] std::mt19937& rng() const;
  [[nodiscard]] ScopedLog& log() const;
  [[nodiscard]] DomainEventBus& event_bus() const;
  [[nodiscard]] const SectorTuning& sector() const;
  [[nodiscard]] float dt() const;
  [[nodiscard]] EntityCtx entity_context(entt::entity self) const;

private:
  AccountCtx* account_;
  const SectorTuning* sector_;
  float dt_seconds_;
};

class EntityCtx {
public:
  EntityCtx(SectorTickCtx tick, entt::entity self);

  [[nodiscard]] entt::registry& registry() const;
  [[nodiscard]] std::mt19937& rng() const;
  [[nodiscard]] ScopedLog& log() const;
  [[nodiscard]] DomainEventBus& event_bus() const;
  [[nodiscard]] const SectorTuning& sector() const;
  [[nodiscard]] float dt() const;
  [[nodiscard]] entt::entity self() const;
  [[nodiscard]] EntityCtx entity_context(entt::entity self) const;

  template <typename Component>
  [[nodiscard]] Component& get() const {
    return registry().get<Component>(self_);
  }

  template <typename Component>
  [[nodiscard]] const Component& get_const() const {
    return registry().get<Component>(self_);
  }

private:
  SectorTickCtx tick_;
  entt::entity self_;
};

}  // namespace hyperverse
