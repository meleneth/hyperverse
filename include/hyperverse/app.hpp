#pragma once

#include "hyperverse/account_context.hpp"

namespace hyperverse {

class App {
public:
  [[nodiscard]] int run(AccountCtx& account);
};

}  // namespace hyperverse
