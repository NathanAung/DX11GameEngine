# Step Engine

<img src="https://i.imgur.com/8bdSI8v.jpeg" alt="Step Engine" referrerpolicy="no-referrer">

**Step Engine** is a modular 3D game engine written from scratch in **C++17** and **DirectX 11**. It compiles from a single codebase into two distinct targets:

- **`DX11Editor`** — A full visual authoring environment powered by **Dear ImGui (Docking)** and **ImGuizmo**, featuring live scene editing, asset management, non-destructive playtesting, and one-click game packaging.
- **`DX11Runtime`** — A lightweight standalone game executable compiled with the `DIST_BUILD` preprocessor macro that strips all editor UI, mounts a packed binary `.pak` Virtual File System (VFS), and boots directly into gameplay.

---

## Key Features

| Subsystem | Capabilities |
| :--- | :--- |
| **Forward PBR & CSM Renderer** | DirectX 11 hardware pipeline with Cook-Torrance microfacet BRDF (GGX, Smith Schlick-GGX, Fresnel-Schlick), tangent-space normal mapping, 3-cascade shadow maps (2048x2048 `Texture2DArray` with 3x3 PCF and texel snapping), up to 16 frustum-culled lights, skybox cubemaps, and off-screen editor framebuffers. |
| **Entity Component System (ECS)** | Cache-friendly sparse-set registry via **EnTT**, 14 built-in components, intrusive doubly-linked parent-child entity hierarchies (`RelationshipComponent`), cycle-checked reparenting, and deferred entity destruction. |
| **Multi-Threaded Physics** | **Jolt Physics** integration running on `hardware_concurrency - 1` worker threads, supporting Box, Sphere, Capsule, concave static `MeshShape`, and dynamic `ConvexHullShape` colliders, raycasting with OBB fallback, and thread-safe contact callbacks. |
| **Sandboxed Lua Scripting** | Embedded **Lua** runtime bound via **sol2**, supporting multiple scripts per entity with isolated `sol::environment` sandboxes, lifecycle hooks (`OnCreate`, `OnUpdate`, `OnDestroy`), collision callbacks (`OnCollisionEnter`, `OnCollisionExit`), and error-protected execution. |
| **Spatial Audio** | **miniaudio** backend supporting 2D UI/music playback and 3D spatialized emitters synchronized each frame with the active camera listener and entity world transforms. |
| **2D UI & Typography** | Orthographic screen-space overlay pass for 2D sprites (`SpriteRendererComponent`) and dynamic TrueType text (`TextRendererComponent`) baked at runtime into 2048x2048 RGBA glyph atlases via `stb_truetype`. |
| **UUID Assets, VFS & Export** | Persistent 64-bit `Engine::UUID` asset ledger (`enginefiles/AssetRegistry.json`), RapidJSON 4-pass scene serializer, `PAK2` binary archive packer/VFS mounter, and automated `ExportedGame/` packaging. |

---

## Repository Structure

```text
DX11GameEngine/
├── assets/                 # Game content (Audio, Fonts, Models, Scripts, Textures, Scenes)
├── cmake/                  # Shared CMake helper modules (BuildOptions.cmake)
├── docs/                   # MkDocs technical documentation
├── enginefiles/            # Engine metadata (AssetRegistry.json, Launch.txt)
├── external/imguizmo/      # Vendored ImGuizmo 3D gizmo library
├── include/Engine/         # Engine C++17 public header files
├── shaders/                # HLSL Shader Model 5.0 vertex and pixel shaders
├── src/                    # Application entry point (main.cpp) and Engine implementations
├── tools/                  # Setup and build scripts (SetupProject.bat, BuildGame.bat, VSCode.bat)
├── CMakeLists.txt          # Dual-target CMake configuration (DX11Editor & DX11Runtime)
├── CMakeSettings.json      # Visual Studio CMake presets (x64-Debug, x64-Release)
├── mkdocs.yml              # MkDocs Material configuration
└── vcpkg.json              # vcpkg manifest dependencies
```

---

## Technology Stack & Dependencies

All external libraries (except Windows SDK DirectX libraries and vendored `ImGuizmo`) are declared in `vcpkg.json` and resolved automatically through **vcpkg manifest mode**:

| Dependency | Source | Role in Engine |
| :--- | :--- | :--- |
| **Direct3D 11** (`d3d11`, `dxgi`, `d3dcompiler`, `dxguid`) | Windows SDK | GPU rendering backend and HLSL compiler |
| **SDL2** | `vcpkg` | Win32 window creation, OS event loop, high-resolution timer, keyboard/mouse input |
| **EnTT** | `vcpkg` | Sparse-set Entity Component System (`entt::registry`) |
| **Jolt Physics** | `vcpkg` | Rigid-body simulation, collision shapes, narrow-phase raycasting |
| **Assimp** | `vcpkg` | 3D model importing (`.obj`, `.fbx`, `.gltf`, `.glb`) with custom VFS streams |
| **Dear ImGui** (`1.91.8`) | `vcpkg` | Dockable editor UI (`dx11-binding`, `sdl2-binding`, `docking-experimental`) |
| **ImGuizmo** | `external/imguizmo/` | Interactive 3D Translate, Rotate, and Scale viewport gizmos |
| **Lua** & **sol2** | `vcpkg` | Gameplay scripting VM and C++ usertype/enum bindings |
| **RapidJSON** | `vcpkg` | JSON scene serialization and `AssetRegistry.json` persistence |
| **miniaudio** | `vcpkg` | 2D and 3D spatial audio playback and in-memory VFS decoding |
| **stb** (`stb_image`, `stb_truetype`) | `vcpkg` | Image decoding (`RGBA8`) and `.ttf` font atlas baking |

---

## Quick-Start Build Instructions

### Prerequisites

- **OS**: Windows 10 or Windows 11 (x64) with DirectX 11 runtime
- **Compiler / IDE**: Visual Studio 2022 with the **Desktop development with C++** workload (MSVC v143, Windows 10/11 SDK, C++ CMake tools for Windows)
- **Build Tools**: CMake 3.20+ and `vcpkg` (can be installed automatically via `tools\SetupProject.bat`)

### Option A: Visual Studio 2022 (Recommended)

1. **Bootstrap CMake & vcpkg (if not already installed)**:
   Run the automated setup batch file from the repository root:
   ```bat
   tools\SetupProject.bat
   ```
   This installs CMake to `%LOCALAPPDATA%\w64cmake` and clones/bootstraps `vcpkg` at `%SystemDrive%\vcpkg` (verifying `%SystemDrive%\vcpkg\scripts\buildsystems\vcpkg.cmake`).

2. **Open the Folder in Visual Studio**:
   Launch Visual Studio 2022 and select **File -> Open -> Folder...**, choosing the `DX11GameEngine` root directory. Visual Studio detects `CMakeLists.txt` and `CMakeSettings.json` automatically.

3. **Configure CMake & Install Dependencies**:
   Select `x64-Debug` or `x64-Release` in the configuration dropdown. Ensure `cmakeToolchain` in `CMakeSettings.json` points to `${env.SystemDrive}/vcpkg/scripts/buildsystems/vcpkg.cmake`. On first run, `vcpkg` downloads and builds all packages in `vcpkg.json`.

4. **Build Both Targets**:
   Select **Build -> Build All**.
   - Building runs custom targets `CopyShaders`, `CopyAssets`, and `CopyEngineFiles`, mirroring `shaders/`, `assets/`, and `enginefiles/` into `${CMAKE_BINARY_DIR}/bin/`.
   - Building `DX11Runtime` triggers a `POST_BUILD` step that populates `bin/RuntimeTemplate/` with `DX11Runtime.exe` and all required `.dll` files, enabling the Editor's **Export Game** button.

5. **Run the Editor**:
   Ensure `DX11Editor.exe` is selected as the startup target and press `Ctrl + F5` (**Debug -> Start Without Debugging**).

### Option B: Command-Line CMake Build

You can configure and build both executables directly from a terminal (or run `tools\BuildGame.bat`):

```powershell
cmake -S . -B out/build/x64-Release -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:SystemDrive/vcpkg/scripts/buildsystems/vcpkg.cmake"

cmake --build out/build/x64-Release --config Release
```

Compiled binaries are placed in `out/build/x64-Release/bin/Release/` (`DX11Editor.exe` and `DX11Runtime.exe`).

---

## Documentation Map

### Architecture
- [Core Engine Loop & Lifecycle](architecture/engine-loop.md) — Initialization, frame update order, `Edit <-> Play` state machine, and dual build targets.
- [ECS & Entity Management](architecture/ecs.md) — EnTT registry design, intrusive entity hierarchies, transform propagation, and deferred destruction.
- [Rendering Pipeline & Shaders](architecture/rendering-pipeline.md) — Multi-pass DirectX 11 architecture, PBR BRDF, Cascaded Shadow Maps, skybox, and 2D UI.
- [Subsystems, Physics, Audio & VFS](architecture/subsystems.md) — Jolt Physics integration, Lua sandboxing, spatial audio, UUID asset tracking, and `PAK2` VFS packaging.

### API Reference
- [Core, Scene, Systems & Serialization](api/core-and-scene.md) — `UUID`, `Engine::Math`, `Scene`, `SceneSerializer`, and ECS system functions.
- [ECS Components & Enums](api/components.md) — Exact struct layouts, field types, defaults, and enums from `Components.h`.
- [Renderer, Managers & Editor UI](api/managers-and-renderer.md) — `Renderer`, `AssetManager`, `MeshManager`, `ShaderManager`, `TextureManager`, `FontManager`, `PhysicsManager`, `AudioManager`, `InputManager`, `ImGuiManager`, and `EditorUI`.
- [Lua Scripting Bindings](api/lua-scripting.md) — Complete Lua usertype, global table, callback, and enum reference from `ScriptEntity.h` and `Scene::InitializeLuaBindings`.

### Developer Guides
- [Adding & Managing Assets](guides/adding-assets.md) — Importing 3D models, PBR texture maps, audio clips, TrueType fonts, and skyboxes.
- [Creating, Editing & Exporting Scenes](guides/creating-scenes.md) — Viewport navigation, gizmos, Hierarchy/Inspector workflows, JSON scene saving, and standalone game export.
- [Writing Lua Gameplay Scripts](guides/writing-scripts.md) — Script lifecycle, movement, spawning, raycasting, collisions, UI HUD scripting, and common pitfalls.
