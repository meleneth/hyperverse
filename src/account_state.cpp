#include "hyperverse/account_state.hpp"

#include <utility>

namespace hyperverse {

AccountState::AccountState(std::string callsign) : callsign_{std::move(callsign)} {}

const std::string& AccountState::callsign() const {
  return callsign_;
}

}  // namespace hyperverse
