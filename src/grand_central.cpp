#include "hyperverse/grand_central.hpp"

#include <ostream>
#include <utility>

namespace hyperverse {

ScopedLog::ScopedLog(std::ostream& output, std::string scope) : output_{&output}, scope_{std::move(scope)} {}

void ScopedLog::info(const std::string& message) const {
  *output_ << "[" << scope_ << "] " << message << '\n';
}

const std::string& ScopedLog::scope() const {
  return scope_;
}

AccountState::AccountState(std::string callsign) : callsign_{std::move(callsign)} {}

const std::string& AccountState::callsign() const {
  return callsign_;
}

AccountCtx::AccountCtx(entt::registry& registry, std::mt19937& rng, ScopedLog& log, AccountState& account)
    : registry_{&registry}, rng_{&rng}, log_{&log}, account_{&account} {}

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

GrandCentral::GrandCentral(std::ostream& log_output)
    : rng_{0x48595045U}, log_{log_output, "account"}, account_{"Pioneer"} {}

AccountCtx GrandCentral::account_context() {
  return {registry_, rng_, log_, account_};
}

}  // namespace hyperverse
