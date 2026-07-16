cmake_minimum_required(VERSION 3.28)

project(
  hyperverse
  VERSION 0.1.0
  DESCRIPTION "Gamepad-first asteroid mining and cargo escort prototype"
  LANGUAGES CXX
)

option(HYPERVERSE_BUILD_TESTS "Build Hyperverse tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CPM)
include(CompilerWarnings)

find_package(Vulkan REQUIRED)
find_package(PNG REQUIRED)

CPMAddPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.4.12
  OPTIONS
    "SDL_TEST_LIBRARY OFF"
)

CPMAddPackage(
  NAME EnTT
  GITHUB_REPOSITORY skypjack/entt
  GIT_TAG v3.16.0
)
set_property(TARGET EnTT PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${EnTT_SOURCE_DIR}/src")

CPMAddPackage(
  NAME JoltPhysics
  GITHUB_REPOSITORY jrouwe/JoltPhysics
  GIT_TAG v5.6.0
  SOURCE_SUBDIR Build
  OPTIONS
    "JPH_USE_DX12 OFF"
    "JPH_USE_VK OFF"
    "JPH_USE_MTL OFF"
    "TARGET_HELLO_WORLD OFF"
    "TARGET_PERFORMANCE_TEST OFF"
    "TARGET_SAMPLES OFF"
    "TARGET_UNIT_TESTS OFF"
    "TARGET_VIEWER OFF"
)

CPMAddPackage(
  NAME BoostSML
  GITHUB_REPOSITORY boost-ext/sml
  GIT_TAG v1.2.0
  DOWNLOAD_ONLY YES
)

CPMAddPackage(
  NAME EventPP
  GITHUB_REPOSITORY wqking/eventpp
  GIT_TAG v0.1.3
  DOWNLOAD_ONLY YES
)

add_library(hyperverse_boost_sml INTERFACE)
add_library(hyperverse::boost_sml ALIAS hyperverse_boost_sml)
target_include_directories(hyperverse_boost_sml SYSTEM INTERFACE "${BoostSML_SOURCE_DIR}/include")

add_library(hyperverse_eventpp INTERFACE)
add_library(hyperverse::eventpp ALIAS hyperverse_eventpp)
target_include_directories(hyperverse_eventpp SYSTEM INTERFACE "${EventPP_SOURCE_DIR}/include")

if(TARGET SDL3::SDL3)
  set(HYPERVERSE_SDL_TARGET SDL3::SDL3)
elseif(TARGET SDL3::SDL3-shared)
  set(HYPERVERSE_SDL_TARGET SDL3::SDL3-shared)
elseif(TARGET SDL3::SDL3-static)
  set(HYPERVERSE_SDL_TARGET SDL3::SDL3-static)
else()
  message(FATAL_ERROR "SDL3 target was not created by dependency setup")
endif()

if(TARGET Jolt)
  set(HYPERVERSE_JOLT_TARGET Jolt)
elseif(TARGET Jolt::Jolt)
  set(HYPERVERSE_JOLT_TARGET Jolt::Jolt)
else()
  message(FATAL_ERROR "Jolt target was not created by dependency setup")
endif()

add_library(
  hyperverse_core
  src/account_context.cpp
  src/account_state.cpp
  src/app.cpp
  src/camera.cpp
  src/cargo_box.cpp
  src/cargo_escort.cpp
  src/cargo_manifest.cpp
  src/cargo_route.cpp
  src/cargo_train.cpp
  src/collision.cpp
  src/drone.cpp
  src/fixed_timestep.cpp
  src/flight.cpp
  src/grand_central.cpp
  src/hud_title.cpp
  src/input.cpp
  src/mining.cpp
  src/physics.cpp
  src/pressure.cpp
  src/projectile.cpp
  src/raider.cpp
  src/render_frame.cpp
  src/sector.cpp
  src/scoped_log.cpp
  src/sdl_platform.cpp
  src/sphere_queries.cpp
  src/targeting.cpp
  src/sprite_frame_builder.cpp
  src/version.cpp
  src/vertical_slice_seed.cpp
  src/vulkan_assets.cpp
  src/vulkan_commands.cpp
  src/vulkan_pipeline.cpp
  src/vulkan_renderer.cpp
  src/vulkan_swapchain.cpp
  src/vulkan_texture.cpp
)
add_library(hyperverse::core ALIAS hyperverse_core)
target_include_directories(
  hyperverse_core
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(
  hyperverse_core
  PUBLIC
    EnTT::EnTT
    hyperverse::boost_sml
    hyperverse::eventpp
  PRIVATE
    ${HYPERVERSE_SDL_TARGET}
    PNG::PNG
    Vulkan::Vulkan
    ${HYPERVERSE_JOLT_TARGET}
)
hyperverse_enable_project_warnings(hyperverse_core)

add_executable(hyperverse src/main.cpp)
target_link_libraries(hyperverse PRIVATE hyperverse::core)
hyperverse_enable_project_warnings(hyperverse)
add_custom_command(
  TARGET hyperverse
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory
    "${PROJECT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:hyperverse>/assets"
  COMMENT "Copying runtime assets next to hyperverse"
  VERBATIM
)

install(TARGETS hyperverse RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY assets/ DESTINATION share/hyperverse/assets)

if(HYPERVERSE_BUILD_TESTS)
  include(CTest)
  CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.8.1
  )
  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
  include(Catch)

  add_executable(
    hyperverse_tests
    tests/architecture_tests.cpp
    tests/bootstrap_tests.cpp
    tests/cargo_tests.cpp
    tests/collision_tests.cpp
    tests/drone_tests.cpp
    tests/fixed_timestep_tests.cpp
    tests/flight_tests.cpp
    tests/input_tests.cpp
    tests/mining_tests.cpp
    tests/pressure_tests.cpp
    tests/projectile_tests.cpp
    tests/raider_tests.cpp
    tests/render_frame_tests.cpp
    tests/sector_tests.cpp
    tests/targeting_tests.cpp
  )
  target_link_libraries(hyperverse_tests PRIVATE hyperverse::core Catch2::Catch2WithMain)
  hyperverse_enable_project_warnings(hyperverse_tests)
  catch_discover_tests(hyperverse_tests)
endif()
