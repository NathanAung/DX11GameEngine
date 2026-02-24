cmake_minimum_required(VERSION 3.20)

project(DX11GameEngine
    VERSION 0.1.0
    LANGUAGES CXX
)

# --------------------------------------------------------------
# Compiler and project settings
# --------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/permissive- /W4 /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_HAS_CXX17=1)

    # Better IntelliSense via external includes
    add_compile_options(/external:W0 /external:anglebrackets)
    set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "/external:I")
endif()

# Output directory (bin/Debug, bin/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --------------------------------------------------------------
# Source files
# --------------------------------------------------------------

set(ENGINE_SOURCE_FILES
    src/main.cpp
    src/Engine/Renderer.cpp
    src/Engine/InputManager.cpp
    src/Engine/Scene.cpp
    src/Engine/MeshManager.cpp
    src/Engine/ShaderManager.cpp
    src/Engine/TextureManager.cpp
    src/Engine/Systems.cpp
    src/Engine/PhysicsManager.cpp
    src/Engine/ImGuiManager.cpp
    src/Engine/EditorUI.cpp
)

set(ENGINE_HEADER_FILES
    include/Engine/Core.h
    include/Engine/Renderer.h
    include/Engine/InputManager.h
    include/Engine/Components.h
    include/Engine/Scene.h
    include/Engine/MeshManager.h
    include/Engine/ShaderManager.h
    include/Engine/TextureManager.h
    include/Engine/Systems.h
    include/Engine/PhysicsManager.h
    include/Engine/ImGuiManager.h
    include/Engine/MathUtils.h
    include/Engine/EditorUI.h
)

add_executable(DX11GameEngine
    ${ENGINE_SOURCE_FILES}
    ${ENGINE_HEADER_FILES}
)

# Precompiled header (optional)
target_precompile_headers(DX11GameEngine PRIVATE include/Engine/Core.h)

# --------------------------------------------------------------
# vcpkg manifest mode packages (auto-installed)
# --------------------------------------------------------------

# SDL2
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(DX11GameEngine PRIVATE SDL2::SDL2 SDL2::SDL2main)

# Assimp
find_package(assimp CONFIG REQUIRED)
target_link_libraries(DX11GameEngine PRIVATE assimp::assimp)

# EnTT
find_package(EnTT CONFIG REQUIRED)
target_link_libraries(DX11GameEngine PRIVATE EnTT::EnTT)

# RapidJSON (header-only)
find_package(RapidJSON CONFIG REQUIRED)

# imgui (DX11 backend)
find_package(imgui CONFIG REQUIRED)
target_link_libraries(DX11GameEngine PRIVATE imgui::imgui)

# Jolt Physics
find_package(Jolt CONFIG REQUIRED)
target_link_libraries(DX11GameEngine PRIVATE Jolt::Jolt)

# --------------------------------------------------------------
# DirectX system libraries
# --------------------------------------------------------------

if (WIN32)
    target_link_libraries(DX11GameEngine PRIVATE
        d3d11
        dxgi
        d3dcompiler
        dxguid
    )
endif()

# --------------------------------------------------------------
# Include directories
# --------------------------------------------------------------

target_include_directories(DX11GameEngine
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

# --------------------------------------------------------------
# Visual Studio settings
# --------------------------------------------------------------

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT DX11GameEngine)

set_property(TARGET DX11GameEngine
             PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
             "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# --------------------------------------------------------------
# Copy shaders & assets to build/bin
# --------------------------------------------------------------

add_custom_target(CopyShaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
)

add_custom_target(CopyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)

add_dependencies(DX11GameEngine CopyShaders CopyAssets)

# --------------------------------------------------------------
# IDE File Grouping
# --------------------------------------------------------------

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${ENGINE_SOURCE_FILES} ${ENGINE_HEADER_FILES}
)

message(STATUS "DX11GameEngine configured (manifest mode active).")
