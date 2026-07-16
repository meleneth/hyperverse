#include "hyperverse/grand_central.hpp"

namespace hyperverse {

GrandCentral::GrandCentral(std::ostream& log_output)
    : rng_{0x48595045U}, log_{log_output, "account"}, account_{"Pioneer"} {}

AccountCtx GrandCentral::account_context() {
  return {registry_, rng_, log_, account_, physics_, event_bus_};
}

}  // namespace hyperverse
