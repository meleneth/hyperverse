#pragma once

#include <entt/entity/registry.hpp>

#include <iosfwd>
#include <random>
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

class AccountState {
public:
  explicit AccountState(std::string callsign);

  [[nodiscard]] const std::string& callsign() const;

private:
  std::string callsign_;
};

class AccountCtx {
public:
  AccountCtx(entt::registry& registry, std::mt19937& rng, ScopedLog& log, AccountState& account);

  [[nodiscard]] entt::registry& registry() const;
  [[nodiscard]] std::mt19937& rng() const;
  [[nodiscard]] ScopedLog& log() const;
  [[nodiscard]] AccountState& account() const;

private:
  entt::registry* registry_;
  std::mt19937* rng_;
  ScopedLog* log_;
  AccountState* account_;
};

class GrandCentral {
public:
  explicit GrandCentral(std::ostream& log_output);

  [[nodiscard]] AccountCtx account_context();

private:
  entt::registry registry_;
  std::mt19937 rng_;
  ScopedLog log_;
  AccountState account_;
};

}  // namespace hyperverse
