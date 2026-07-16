#pragma once

#include <iosfwd>
#include <string>

namespace hyperverse {

class ScopedLog {
public:
  ScopedLog(std::ostream& output, std::string scope);

  void info(const std::string& message) const;
  [[nodiscard]] const std::string& scope() const;

private:
  std::ostream* output_;
  std::string scope_;
};

}  // namespace hyperverse
