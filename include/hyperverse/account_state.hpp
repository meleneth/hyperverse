#pragma once

#include <string>

namespace hyperverse {

class AccountState {
public:
  explicit AccountState(std::string callsign);

  [[nodiscard]] const std::string& callsign() const;

private:
  std::string callsign_;
};

}  // namespace hyperverse
