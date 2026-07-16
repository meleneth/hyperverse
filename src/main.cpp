#include "hyperverse/app.hpp"
#include "hyperverse/grand_central.hpp"

#include <iostream>

int main() {
  hyperverse::GrandCentral grand_central{std::cout};
  hyperverse::AccountCtx account = grand_central.account_context();
  hyperverse::App app;
  return app.run(account);
}
