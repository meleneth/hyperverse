#include "hyperverse/scoped_log.hpp"

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

}  // namespace hyperverse
