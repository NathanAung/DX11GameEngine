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
    src/Engine/UUID.cpp
    src/Engine/SceneSerializer.cpp
    src/Engine/AssetManager.cpp
    src/Engine/Renderer.cpp
    src/Engine/InputManager.cpp
    src/Engine/Scene.cpp
    src/Engine/MeshManager.cpp
    src/Engine/ShaderManager.cpp
    src/Engine/TextureManager.cpp
    src/Engine/Systems.cpp
    src/Engine/PhysicsManager.cpp
    src/Engine/ImGuiManager.cpp
    src/Engine/AudioManager.cpp
    src/Engine/EditorUI.cpp
    external/imguizmo/ImGuizmo.cpp
)

set(ENGINE_HEADER_FILES
    include/Engine/Core.h
    include/Engine/UUID.h
    include/Engine/SceneSerializer.h
    include/Engine/AssetManager.h
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
    include/Engine/AudioManager.h
    include/Engine/MathUtils.h
    include/Engine/EditorUI.h
    include/Engine/ScriptEntity.h
    external/imguizmo/ImGuizmo.h
)

# Create the Editor executable
add_executable(DX11Editor
    ${ENGINE_SOURCE_FILES}
    ${ENGINE_HEADER_FILES}
)

# Create the Standalone Game executable
add_executable(DX11Runtime
    ${ENGINE_SOURCE_FILES}
    ${ENGINE_HEADER_FILES}
)

# Inject the DIST_BUILD macro ONLY into the Runtime target
target_compile_definitions(DX11Runtime PRIVATE DIST_BUILD)

# Precompiled headers
target_precompile_headers(DX11Editor PRIVATE include/Engine/Core.h)
target_precompile_headers(DX11Runtime PRIVATE include/Engine/Core.h)

# --------------------------------------------------------------
# vcpkg manifest mode packages (auto-installed)
# --------------------------------------------------------------

# --------------------------------------------------------------
# vcpkg manifest mode packages & linking
# --------------------------------------------------------------

# Find all required packages
find_package(SDL2 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(lua CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR "miniaudio.h" REQUIRED)
find_path(RAPIDJSON_INCLUDE_DIR "rapidjson/document.h" REQUIRED)

# Apply linking and includes to BOTH targets
set(ENGINE_TARGETS DX11Editor DX11Runtime)

foreach(TARGET IN LISTS ENGINE_TARGETS)
    target_link_libraries(${TARGET} PRIVATE SDL2::SDL2 SDL2::SDL2main)
    target_link_libraries(${TARGET} PRIVATE assimp::assimp)
    target_link_libraries(${TARGET} PRIVATE EnTT::EnTT)
    target_link_libraries(${TARGET} PRIVATE imgui::imgui)
    target_link_libraries(${TARGET} PRIVATE Jolt::Jolt)
    target_link_libraries(${TARGET} PRIVATE lua)
    target_link_libraries(${TARGET} PRIVATE sol2::sol2)
    
    target_include_directories(${TARGET} PRIVATE ${MINIAUDIO_INCLUDE_DIR})
    target_include_directories(${TARGET} PRIVATE ${RAPIDJSON_INCLUDE_DIR})
    
# --------------------------------------------------------------
# Include directories
# --------------------------------------------------------------

    target_include_directories(${TARGET} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external/imguizmo
    )

# --------------------------------------------------------------
# DirectX system libraries
# --------------------------------------------------------------

    if (WIN32)
        target_link_libraries(${TARGET} PRIVATE d3d11 dxgi d3dcompiler dxguid)
    endif()

    set_property(TARGET ${TARGET} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()

# --------------------------------------------------------------
# Visual Studio settings
# --------------------------------------------------------------

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT DX11Editor)

# --------------------------------------------------------------
# Custom build steps for copying assets and shaders
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

add_custom_target(CopyEngineFiles ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/enginefiles
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/enginefiles
)

# Ensure both targets trigger the asset copying
add_dependencies(DX11Editor CopyShaders CopyAssets CopyEngineFiles)
add_dependencies(DX11Runtime CopyShaders CopyAssets CopyEngineFiles)

# --------------------------------------------------------------
# IDE File Grouping
# --------------------------------------------------------------

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${ENGINE_SOURCE_FILES} ${ENGINE_HEADER_FILES}
)

# --------------------------------------------------------------
# RUNTIME TEMPLATE PACKAGING
# --------------------------------------------------------------
# Automatically place the Standalone executable and its required DLLs 
# into a template folder so the Editor can copy them out later.

set(RUNTIME_TEMPLATE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RuntimeTemplate)

add_custom_command(TARGET DX11Runtime POST_BUILD
    # Create the RuntimeTemplate folder inside bin/
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_TEMPLATE_DIR}
    
    # Copy the DX11Runtime.exe into it
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:DX11Runtime> ${RUNTIME_TEMPLATE_DIR}/

    # Copy all dynamically linked libraries (DLLs) required to run the game
    COMMAND powershell -noprofile -Command "Copy-Item -Path '$<TARGET_FILE_DIR:DX11Runtime>/*.dll' -Destination '${RUNTIME_TEMPLATE_DIR}' -Force -ErrorAction SilentlyContinue"
    
    COMMENT "Assembling Runtime Template at ${RUNTIME_TEMPLATE_DIR}..."
)

message(STATUS "DX11GameEngine (Editor & Runtime) configured.")
