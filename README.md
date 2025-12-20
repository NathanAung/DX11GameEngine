# StepEngine

<img src="https://i.imgur.com/8bdSI8v.jpeg"/>

## Introduction
A small DirectX 11 game engine written in C/C++ with a focus on learning modern game engine architecture, graphics programming, entity-component systems, resource management, and physics integration. The engine provides a lightweight foundation with a forward renderer (basic lights), physics (Jolt), and tooling-ready subsystems with an emphasis on clarity and extensibility. PBR parameters are supported at the material level. This project is still a work in progress.

---

## License

Please view the license before looking at the repository.

---

## Languages used

This project is primarily implemented in:
- C++
- C
- CMake
- HLSL

Notes:
- Core engine and rendering code are in C++17.
- Some low-level helpers and C APIs are used where appropriate.
- CMake is used to configure and build the project with vcpkg (manifest mode).
- HLSL is used for Direct3D 11 vertex/pixel shaders.

---

## Libraries and tools used

The engine depends on several open-source libraries and tools. Exact versions are managed via vcpkg manifest mode and referenced by CMake.

- SDL2 — window creation and input handling on Windows
- DirectX 11 (D3D11) — GPU rendering backend
- Jolt Physics — rigid body dynamics and collision detection
- Assimp — model asset import (.fbx, .obj, etc.)
- stb_image — image loading
- EnTT — ECS (entity-component system)
- RapidJSON — JSON parsing (planned)
- Dear ImGui (DX11 backend) — editor/UI (planned)
- CMake — build system
- Visual Studio / MSVC — development environment on Windows
- git — version control

---

## Systems — Implementation Details

Below is a summary of how major engine subsystems are implemented and interact.

### Window rendering and user input with SDL

- SDL2 creates the main application window and handles the message loop.
- Platform: currently Windows-only (D3D11 backend). SDL provides consistent keyboard/mouse input capture on Windows.
- SDL responsibilities:
  - Creating the OS window and handling window events (resize, minimize, close).
  - Providing raw input events for keyboard and mouse.
  - Supplying a high-resolution timer used by the engine’s main loop.
- Integration notes:
  - An SDL window is created and the native `HWND` is extracted via `SDL_SysWMinfo`; D3D11 is initialized using the `HWND`.
  - The main loop polls SDL events each frame and forwards them to the input subsystem (`InputManager`) which maps events to engine-level input (key bindings, mouse delta, scroll).
  - Window resize events trigger resizing of swap chain buffers and recreation of render targets and viewport state.

### Graphics rendering pipeline, camera and skybox

- Renderer backend: Direct3D 11 (D3D11). The renderer wraps device, device context, swap chain, and render targets into a `Renderer` class.
- Typical frame flow:
  1. Update CPU-side scene (transforms, simple animations).
  2. Upload per-frame constant buffers (camera matrices, lighting).
  3. Build draw lists.
  4. Execute draw calls.
  5. Present via swap chain.
- Camera:
  - Uses classic view and projection matrices. The camera exposes world transform, view matrix (look-at), and inverse matrices for shader usage.
  - Perspective projection and an editor-style camera controller are supported.
- Skybox:
  - Implemented as a cube mesh drawn with a specialized skybox shader. The skybox uses a cubemap texture and depth-state configuration so the sky renders behind everything.

### Entity Component System (ECS)

- The engine uses a lightweight component-based architecture with EnTT:
  - Entities: small integer IDs (or handles) representing objects in the world.
  - Components: small POD structs (Transform, MeshRenderer, Rigidbody, CameraComponent, LightComponent, etc.) stored in EnTT sparse sets for cache-friendly iteration.
  - Systems: functions that iterate over entities with specific components and perform work (rendering, physics sync, animation, input).
- Implementation highlights:
  - Entity lifecycle: create/destroy APIs, component add/remove support.
  - Systems are called each frame in a deterministic order (physics → animation → script → render).
- The ECS favors simplicity and explicitness for easy reading and modification.

### Resource Loading (textures and models)

- Managers:
  - MeshManager: procedural primitives and Assimp-based model import.
  - TextureManager: image loading and SRV creation with caching.
  - ShaderManager: shader compilation/binding and input layouts.
- Textures:
  - Loaded via `stb_image` for common formats (PNG, JPG).
  - Currently uploaded as RGBA8 UNORM textures with a single mip level.
- Models:
  - Meshes are imported with Assimp; per-mesh vertex attributes are parsed and converted into engine mesh buffers.
- Materials:
  - Material data (basic PBR parameters: metallic and roughness, plus optional texture SRV) are CPU-side and uploaded to GPU via constant buffers at draw time.

### Mesh creation (box, sphere, capsule and model mesh)

- Primitive mesh generators:
  - Box
  - Sphere
  - Capsule
- Model mesh:
  - Imported via Assimp. Per-vertex attributes (position, normal, tangent, uv) are stored in mesh buffers.
- Mesh representation:
  - Vertex buffers and index buffers are created as D3D11 buffers and managed by the MeshManager.
  - Each mesh has draw metadata (index count, topology); bounding volumes are used where needed.

### Lighting and materials

- Light types: directional, point, spot (constant buffer supports multiple lights).
- Material parameters: metallic and roughness; texture SRV bindable to PS t0.
- PBR note: Current shading uses material parameters and light constants in a forward renderer.

### Physics (Jolt integration, rigidbody creation)

- Jolt Physics is integrated as the physics backend:
  - The engine creates a Jolt Physics world and advances it each simulation tick.
  - Rigid bodies:
    - Created from engine entities with a `RigidBodyComponent` (mass, collision shape, motion type).
    - Collision shapes: built from primitive shapes (box, sphere, capsule) or mesh-based colliders generated from imported model geometry.
  - Synchronization:
    - After stepping the simulation, transforms for dynamic bodies are read back from Jolt and written to ECS components.

### CMake setup

- The project uses CMake to configure builds and vcpkg (manifest mode) to fetch dependencies.
- Top-level `CMakeLists.txt`:
  - Defines a single executable target: `DX11GameEngine`.
  - Uses `find_package()` for SDL2, Assimp, EnTT, RapidJSON, Dear ImGui, and Jolt.
  - Links Windows system libraries: `d3d11`, `dxgi`, `d3dcompiler`, `dxguid`.
  - Copies `shaders/` and `assets/` to the build output directory via custom targets.

---

## To be implemented later

Planned features and systems for future development:

- Hierarchy system (entity parent/child transforms and propagation)
- Editor UI (ImGui-based editor for scene editing, material / asset inspectors)
- Object Picking and Gizmos (selection, transform handles)
- Audio system (spatial audio, playback)
- Lua scripting (embed Lua for fast iteration and game logic)
- Data persistence and export (scene/asset serialization, project export pipeline)

---

## How to set up this project (Visual Studio)

This project uses **CMake + vcpkg (manifest mode)**. All third-party dependencies are downloaded automatically during configuration.

### Prerequisites

* **Windows 10/11** (DirectX 11 runtime)
* **Visual Studio 2022**
* **Workload**: Desktop development with C++
* **Components**: MSVC, C++ CMake tools for Windows, Git.

### 1. Install vcpkg

If you don’t already have vcpkg installed:

```bash
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
# (Optional but recommended):
.\vcpkg integrate install
```

*Ensure this file exists: `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`*

### 2. Clone the repository

```bash
git clone https://github.com/NathanAung/DX11GameEngine.git
```

### 3. Open the project in Visual Studio

1. Launch Visual Studio.
2. Select **File → Open → Folder**.
3. Open the cloned `DX11GameEngine` folder.
4. Visual Studio will automatically detect the `CMakeLists.txt`.

### 4. Configure CMake (Visual Studio UI)

1. Go to **Project → CMake Settings**.
2. Select your configuration (e.g., `x64-Debug`).
3. Set the following CMake variable:
   * **Name**: `CMAKE_TOOLCHAIN_FILE`
   * **Value**: `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
4. **Save** the settings.
5. Visual Studio will now configure CMake, invoke vcpkg, and download all dependencies.
   * The first configure may take several minutes.

### 5. Build and Run

1. In the Visual Studio toolbar, select:
   * **Configuration**: `Debug` or `Release`
   * **Architecture**: `x64`
   * **Startup target**: `DX11GameEngine`
2. **Build**: Build → Build All.
3. **Run**: Debug → Start Without Debugging (`Ctrl + F5`).

---

## Demo Videos
### Lighting Demo (Click to play video)
[![Lighting Demo](https://i.imgur.com/0RIBnX6.jpeg)]()
### Galton Board Physics Demo (Click to play video)
[![Galton Board Physics Demo](https://i.imgur.com/Bj7wcyO.jpeg)]()
### Wall Smasher Game Demo (Click to play video)
[![Wall Smasher Game Demo](https://i.imgur.com/jCIpugV.jpeg)]()
### 3D Model Demo (Click to play video)
[![3D Model Demo](https://i.imgur.com/4AC6h1Z.jpeg)]()

##  Gallery
### PBR Lighting
- PBR Lighting and material with metallic/roughness values
<img src="https://i.imgur.com/FqLpL3O.jpeg"/>

- Directional, spot and point lights
<img src="https://i.imgur.com/fT8hAZw.jpeg"/>

### 3D Model Loading and Rendering
<img src="https://i.imgur.com/qVgbAAt.jpeg"/>

<img src="https://i.imgur.com/nliMOQQ.jpeg"/>

---
