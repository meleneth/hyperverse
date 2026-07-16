#pragma once

#include "hyperverse/account_context.hpp"

#include <entt/entity/entity.hpp>

#include <vector>

namespace hyperverse {

struct VerticalSliceEntities {
  entt::entity player{entt::null};
  std::vector<entt::entity> mining_drones{};
  entt::entity raider{entt::null};
};

[[nodiscard]] VerticalSliceEntities seed_vertical_slice(AccountCtx& account);

}  // namespace hyperverse
