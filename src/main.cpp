#include "hyperverse/app.hpp"
#include "hyperverse/grand_central.hpp"

#include <iostream>

int main() {
#if defined(__EMSCRIPTEN__)
  static hyperverse::GrandCentral grand_central{std::cout};
#else
  hyperverse::GrandCentral grand_central{std::cout};
#endif
  hyperverse::AccountCtx account = grand_central.account_context();
  hyperverse::App app;
  return app.run(account);
}
