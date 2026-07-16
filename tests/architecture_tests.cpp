#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("test account world derives a minimal account context without application ownership") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();

  const entt::entity entity = account.registry().create();
  auto* physics = &account.physics();
  account.log().info(account.account().callsign());

  CHECK((entity != entt::null));
  CHECK(physics != nullptr);
  CHECK(&account.event_bus() != nullptr);
  CHECK(account.log().scope() == "account");
  CHECK(world.output.str().find("[account] Pioneer") != std::string::npos);
}

TEST_CASE("composition root header is only included by startup and its implementation") {
  const std::filesystem::path test_file{__FILE__};
  const std::filesystem::path repo_root = test_file.parent_path().parent_path();
  const std::string forbidden_include = "#include \"hyperverse/" "grand_central.hpp\"";
  const std::vector<std::filesystem::path> roots{
    repo_root / "include",
    repo_root / "src",
    repo_root / "tests",
  };

  for (const std::filesystem::path& root : roots) {
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator{root}) {
      if (!entry.is_regular_file()) {
        continue;
      }

      const std::filesystem::path path = entry.path();
      if (path.extension() != ".hpp" && path.extension() != ".cpp") {
        continue;
      }

      std::ifstream input{path};
      const std::string contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
      const bool allowed_startup_or_implementation = path.filename() == "main.cpp" || path.filename() == "grand_central.cpp";
      INFO(path.string());
      CHECK((allowed_startup_or_implementation || contents.find(forbidden_include) == std::string::npos));
    }
  }
}
