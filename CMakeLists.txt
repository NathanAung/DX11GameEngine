cmake_minimum_required(VERSION 3.20)

project(DX11GameEngine
  VERSION 0.1.0
  LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  # Make __cplusplus report the real standard and use conforming mode
  add_compile_options(/permissive- /Zc:__cplusplus)

  # Suppress warnings from external headers
  add_compile_options(/external:W0 /external:anglebrackets)
  # Tell CMake to pass external include dirs as /external:I
  set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "/external:I")

  # Help IntelliSense recognize C++17 library features (string_view, etc.)
  add_compile_definitions(_HAS_CXX17=1)
endif()

include(cmake/BuildOptions.cmake)

# Define where ALL executables and DLLs go for ALL configurations (Debug/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Prefer vcpkg if available: find CONFIG packages.
# SDL2 is required at this step.
set(ENGINE_ENABLE_VCPKG ON CACHE BOOL "Use vcpkg-config packages when available")

if(ENGINE_ENABLE_VCPKG)
  find_package(SDL2 CONFIG QUIET)
  # Header-only libraries (optional via vcpkg)
  find_package(EnTT CONFIG QUIET)
  find_package(RapidJSON CONFIG QUIET)
  find_package(imgui CONFIG QUIET)     # integrate rendering later
  find_package(assimp CONFIG QUIET)    # Used in later steps
endif()

# Fallback for SDL2 if not found via vcpkg:
if(NOT SDL2_FOUND)
  message(STATUS "SDL2 CONFIG not found. You can install 'sdl2:x64-windows' via vcpkg.")
  # As a minimum, allow an environment or cache variable SDL2_DIR to point to a prebuilt SDL2.
  # Expect SDL2_DIR to contain lib/cmake/SDL2 or SDL2Config.cmake.
  if(DEFINED ENV{SDL2_DIR})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{SDL2_DIR}")
    find_package(SDL2 CONFIG REQUIRED)
  elseif(DEFINED SDL2_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${SDL2_DIR}")
    find_package(SDL2 CONFIG REQUIRED)
  else()
    message(FATAL_ERROR "SDL2 not found. Set SDL2_DIR or install via vcpkg and set the vcpkg toolchain.")
  endif()
endif()

# Add source files
set(ENGINE_SOURCE_FILES
  src/main.cpp
  src/Engine/Renderer.cpp
  src/Engine/InputManager.cpp
  src/Engine/Scene.cpp
  src/Engine/MeshManager.cpp
  src/Engine/ShaderManager.cpp
  src/Engine/Systems.cpp
)

set(ENGINE_HEADER_FILES
  include/Engine/Core.h
  include/Engine/Renderer.h
  include/Engine/InputManager.h
  include/Engine/Components.h
  include/Engine/Scene.h
  include/Engine/MeshManager.h
  include/Engine/ShaderManager.h
  include/Engine/Systems.h
)

# Create executable target
add_executable(DX11GameEngine ${ENGINE_SOURCE_FILES} ${ENGINE_HEADER_FILES} "include/Engine/Core.h")

# Add Precompiled Headers for the executable target
target_precompile_headers(DX11GameEngine 
    PRIVATE
        "include/Engine/Core.h"
)

# Language, warnings, and OS-specific definitions
target_compile_features(DX11GameEngine PRIVATE cxx_std_14)
if(MSVC)
  target_compile_options(DX11GameEngine PRIVATE /W4 /permissive- /EHsc)
  target_compile_definitions(DX11GameEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Mark third-party include dirs as SYSTEM so /external:* applies
target_include_directories(DX11GameEngine
  PRIVATE ${CMAKE_SOURCE_DIR}/include
  SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/external/entt
    ${CMAKE_SOURCE_DIR}/external/SDL2/include
)

# Link SDL2
# SDL2::SDL2main is needed if its WinMain entry is used; we will use standard main(), but keep linkage ready.
# Some SDL2 packages expose SDL2::SDL2main; some only SDL2::SDL2. Link if available.
# Guard SDL2 targets (optional safety)
if(TARGET SDL2::SDL2main)
  target_link_libraries(DX11GameEngine PRIVATE SDL2::SDL2main)
endif()
if(TARGET SDL2::SDL2)
  target_link_libraries(DX11GameEngine PRIVATE SDL2::SDL2)
endif()

# Find Assimp
find_package(assimp CONFIG REQUIRED)

# Include dirs (only if target doesn’t propagate)
# target_include_directories(DX11GameEngine PRIVATE ${ASSIMP_INCLUDE_DIRS})

# Link Assimp
target_link_libraries(DX11GameEngine PRIVATE assimp::assimp)

# Optional: lock MSVC runtime to DLL form
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Windows/DirectX system libs
if(WIN32)
  target_link_libraries(DX11GameEngine PRIVATE
    d3d11
    dxgi
    d3dcompiler
    dxguid
  )
endif()

# Organize in IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SOURCE_FILES} ${ENGINE_HEADER_FILES})

# Print summary
message(STATUS "DX11GameEngine configured.")
message(STATUS "  SDL2:         ${SDL2_DIR}")
message(STATUS "  Using vcpkg:  ${ENGINE_ENABLE_VCPKG}")

# Make this target the startup project in VS and set working directory
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DX11GameEngine)
set_property(TARGET DX11GameEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Copy shaders next to the executable
add_custom_target(CopyShaders ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/shaders
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
)
add_dependencies(DX11GameEngine CopyShaders)