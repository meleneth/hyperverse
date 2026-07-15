#include "hyperverse/fixed_timestep.hpp"

#include <algorithm>

namespace hyperverse {

void FixedTimestep::accumulate(float elapsed_seconds) {
  accumulator_ += std::clamp(elapsed_seconds, 0.0F, tick_seconds_ * 8.0F);
}

bool FixedTimestep::consume_tick() {
  if (accumulator_ < tick_seconds_) {
    return false;
  }
  accumulator_ -= tick_seconds_;
  return true;
}

float FixedTimestep::alpha() const {
  return accumulator_ / tick_seconds_;
}

}  // namespace hyperverse
