cmake_minimum_required(VERSION 3.25)

project(
  hyperverse
  VERSION 0.1.0
  DESCRIPTION "Gamepad-first asteroid mining and cargo escort prototype"
  LANGUAGES CXX
)

option(HYPERVERSE_BUILD_TESTS "Build Hyperverse tests" ON)
option(HYPERVERSE_STEAM_BUNDLE "Install a relocatable Steam Linux Runtime bundle" OFF)

if(HYPERVERSE_STEAM_BUNDLE)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(FetchContent)
include(CPM)
include(CompilerWarnings)
find_package(PNG REQUIRED)
find_package(Vulkan 1.2 REQUIRED COMPONENTS glslangValidator)

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
  src/asteroid_fragmentation.cpp
  src/asteroid_geometry.cpp
  src/asteroid_mass.cpp
  src/camera.cpp
  src/cargo_box.cpp
  src/cargo_dispatch.cpp
  src/cargo_escort.cpp
  src/cargo_extraction.cpp
  src/cargo_manifest.cpp
  src/cargo_route.cpp
  src/cargo_train.cpp
  src/collision.cpp
  src/vulkan_renderer.cpp
  src/drone.cpp
  src/engine_trail.cpp
  src/fixed_timestep.cpp
  src/flight.cpp
  src/game_context.cpp
  src/game_session.cpp
  src/gravity_sling.cpp
  src/grand_central.cpp
  src/hud_notice.cpp
  src/input.cpp
  src/mining.cpp
  src/png_rgba.cpp
  src/physics.cpp
  src/player_lifecycle.cpp
  src/pressure.cpp
  src/projectile.cpp
  src/raider.cpp
  src/radar_control.cpp
  src/radar_hud.cpp
  src/render_frame.cpp
  src/sector.cpp
  src/scoped_log.cpp
  src/sdl_platform.cpp
  src/ship_status.cpp
  src/system_menu.cpp
  src/jolt_shape_queries.cpp
  src/sprite_collision_shape_data.cpp
  src/sprite_collision_shape.cpp
  src/targeting.cpp
  src/sprite_frame_builder.cpp
  src/version.cpp
  src/vertical_slice_seed.cpp
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
    Vulkan::Vulkan
    PNG::PNG
    ${HYPERVERSE_JOLT_TARGET}
)
hyperverse_enable_project_warnings(hyperverse_core)

set(HYPERVERSE_SHADER_SOURCES
  sprite.vert sprite.frag
  color.vert color.frag
  triangle.vert triangle.frag
  trail.vert trail.frag
)
set(HYPERVERSE_SHADER_OUTPUTS)
foreach(shader IN LISTS HYPERVERSE_SHADER_SOURCES)
  set(source "${PROJECT_SOURCE_DIR}/shaders/${shader}")
  set(output "${PROJECT_BINARY_DIR}/shaders/${shader}.spv")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/shaders"
    COMMAND Vulkan::glslangValidator -V --target-env vulkan1.2 -o "${output}" "${source}"
    DEPENDS "${source}"
    COMMENT "Compiling Vulkan shader ${shader}"
    VERBATIM
  )
  list(APPEND HYPERVERSE_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(hyperverse_shaders DEPENDS ${HYPERVERSE_SHADER_OUTPUTS})
add_dependencies(hyperverse_core hyperverse_shaders)

add_executable(hyperverse src/main.cpp)
target_link_libraries(hyperverse PRIVATE hyperverse::core)
hyperverse_enable_project_warnings(hyperverse)

if(WIN32)
  add_custom_command(
    TARGET hyperverse
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy -t "$<TARGET_FILE_DIR:hyperverse>"
      "$<TARGET_RUNTIME_DLLS:hyperverse>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying runtime DLLs next to hyperverse"
    VERBATIM
  )

  if(MINGW)
    find_file(HYPERVERSE_VULKAN_LOADER_DLL NAMES vulkan-1.dll PATHS ENV PATH NO_DEFAULT_PATH)
    if(HYPERVERSE_VULKAN_LOADER_DLL)
      add_custom_command(
        TARGET hyperverse
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy_if_different
          "${HYPERVERSE_VULKAN_LOADER_DLL}"
          "$<TARGET_FILE_DIR:hyperverse>"
        COMMENT "Copying Vulkan loader next to hyperverse"
        VERBATIM
      )
    else()
      message(WARNING "vulkan-1.dll was not found on PATH; the Windows runtime may fail until the Vulkan loader is installed or added to PATH.")
    endif()
  endif()
endif()

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

if(HYPERVERSE_STEAM_BUNDLE)
  install(TARGETS hyperverse RUNTIME DESTINATION . COMPONENT HyperverseRuntime)
  install(DIRECTORY assets/ DESTINATION assets COMPONENT HyperverseRuntime)
  install(DIRECTORY "${PROJECT_BINARY_DIR}/shaders/" DESTINATION shaders COMPONENT HyperverseRuntime)
  install(
    FILES "$<TARGET_FILE:${HYPERVERSE_SDL_TARGET}>"
    DESTINATION lib
    COMPONENT HyperverseRuntime
  )
  install(PROGRAMS packaging/steam-runtime/run-hyperverse.sh DESTINATION . COMPONENT HyperverseRuntime)
  install(FILES packaging/steam-runtime/steam-runtime.txt DESTINATION . COMPONENT HyperverseRuntime)
else()
  install(TARGETS hyperverse RUNTIME DESTINATION bin)
  install(DIRECTORY include/ DESTINATION include)
  install(DIRECTORY assets/ DESTINATION share/hyperverse/assets)
  install(DIRECTORY "${PROJECT_BINARY_DIR}/shaders/" DESTINATION share/hyperverse/shaders)
endif()

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
    tests/asteroid_fragmentation_tests.cpp
    tests/asteroid_geometry_tests.cpp
    tests/bootstrap_tests.cpp
    tests/cargo_tests.cpp
    tests/collision_tests.cpp
    tests/drone_tests.cpp
    tests/engine_trail_tests.cpp
    tests/fixed_timestep_tests.cpp
    tests/flight_tests.cpp
    tests/game_session_tests.cpp
    tests/gravity_sling_tests.cpp
    tests/hud_notice_tests.cpp
    tests/input_tests.cpp
    tests/mining_tests.cpp
    tests/pressure_tests.cpp
    tests/player_lifecycle_tests.cpp
    tests/projectile_tests.cpp
    tests/raider_tests.cpp
    tests/radar_hud_tests.cpp
    tests/render_frame_tests.cpp
    tests/sector_tests.cpp
    tests/ship_status_tests.cpp
    tests/system_menu_tests.cpp
    tests/sprite_collision_shape_tests.cpp
    tests/targeting_tests.cpp
  )
  target_link_libraries(hyperverse_tests PRIVATE hyperverse::core Catch2::Catch2WithMain)
  hyperverse_enable_project_warnings(hyperverse_tests)
  catch_discover_tests(hyperverse_tests)
endif()
