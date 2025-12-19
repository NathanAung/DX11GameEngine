# Step Engine
## Introduction
A small DirectX 11 game engine written in C/C++ with a focus on learning modern game engine architecture, graphics programming, entity-component systems, resource management, and physics integration. The engine provides a lightweight foundation for realtime light rendering (PBR), physics (Jolt), and tooling-ready subsystems with an emphasis on clarity and extensibility.

---

## License

Please view the license before looking at the repository.

---

## Languages used

This project is primarily implemented in:
- C++
- C
- HLSL

The core rendering and engine glue is in C++ while some low-level helpers and C APIs are used where appropriate. HLSL is used as the shader language for vertex/fragment shaders.

---

## Libraries and tools used

The engine depends on several open-source libraries and tools. The exact versions can be found in the project CMake configuration or third-party folder.

- SDL2 — Window creation, context and cross-platform input handling
- DirectX 11 (D3D11) — GPU rendering backend
- Jolt Physics — rigid body dynamics and collision detection
- Assimp — model asset import (.fbx, .obj, etc.) (optional / configurable)
- stb_image — image loading and small utilities
- CMake — build system
- Visual Studio / MSVC — development environment on Windows
- git — version control

---

## Systems — Implementation Details

Below is a summary of how major engine subsystems are implemented and interact.

### Window rendering and user input with SDL

- SDL2 creates the main application window and the message loop. Using SDL allows consistent keyboard / mouse input capture on Windows and other platforms.
- SDL is responsible for:
  - Creating the OS window and handling window events (resize, minimize, close).
  - Providing raw input events for keyboard and mouse.
  - Supplying a high-resolution timer used by the engine's main loop.
- Integration notes:
  - The SDL window is created and a D3D11-compatible surface/context is enabled on initialization.
  - The main loop polls SDL events each frame and forwards them to the input subsystem which maps events to engine-level input (key binding, mouse delta, scroll).
  - Window resize events trigger resizing of swap chain buffers and recreation of render targets and viewport state.

### Graphics rendering pipeline, camera and skybox

- Renderer backend: Direct3D 11 (D3D11). The renderer wraps device, device context, swap chain, and render targets into a Renderer class.
- Frame flow:
  1. Update CPU-side scene (animations, transforms).
  2. Upload per-frame constant buffers (camera matrices, lighting).
  3. Perform culling (basic frustum culling) and build draw lists.
  4. Execute draw calls.
  6. Present via swap chain.
- Camera:
  - Implements classic view and projection matrices. The camera exposes world transform, view matrix (look-at), and inverse matrices for shader usage.
  - Currently supports perspective projection and camera controller input.
- Skybox:
  - Implemented as a cube mesh drawn with a specialized skybox shader. The skybox uses a cubemap texture (HDR environment map) rendered with depth test configured so the sky renders behind everything.

### Entity Component System (ECS)

- The engine uses a lightweight component-based architecture Using EnTT. Key concepts:
  - Entities: small integer IDs (or handles) representing objects in the world.
  - Components: small POD structs (Transform, MeshRenderer, RigidbodyHandle, CameraComponent, LightComponent, etc.) stored in contiguous arrays.
  - Systems: functions that iterate over entities with specific components and perform work (rendering, physics sync, animation, input).
- Implementation highlights:
  - Component storage: sparse-set or vector-of-components per component type for cache-friendly iteration.
  - Entity lifecycle: create/destroy APIs, component add/remove support.
  - Systems subscribe to components and are called each frame in a deterministic order (physics -> animation -> script -> render).
- The ECS favors simplicity and explicitness for easy reading and modification.

### Resource Loading (textures and models)

- Resource Manager responsibilities:
  - Centralized loading and lifetime management of textures, meshes, shaders, materials and models.
  - Caching: load-once semantics keyed by identifier.
- Textures:
  - Loaded via stb_image for common formats (PNG, JPG).
  - Textures are uploaded to GPU as D3D11ShaderResourceViews with appropriate settings (sRGB flag for color textures, generate MIP levels as needed).
- Models:
  - Meshes are imported with Assimp and per-mesh vertex attributes are parsed and converted into engine mesh and material assets.
  - Model assets break down to multiple Mesh resources with per-mesh material references.
- Materials:
  - Material data (PBR parameters and texture references) are small CPU-side objects that the renderer converts into GPU shader constants and bound textures at draw time.

### Mesh creation (box, sphere, capsule and model mesh)

- Primitive mesh generators:
  - Box
  - Sphere
  - Capsule
- Model mesh:
  - Imported via Assimp or custom OBJ loader. Per-vertex attributes (position, normal, tangent, uv, bone weights) are stored in mesh buffers.
- Mesh representation:
  - Vertex buffers and index buffers created as D3D11 buffers and managed by the Resource Manager.
  - Each mesh has draw metadata (index format, topology, bounding box/sphere) used for culling and rendering.

### PBR lighting

- The engine implements a metallic-roughness PBR model:
  - Material parameters: albedo (base color), metallic, and roughness.
  - Light types: directional, point, spot.
- Lighting equation:
  - Microfacet BRDF (e.g., Cook-Torrance) with Fresnel (Schlick) and GGX normal distribution.
  - Energy-conserving specular and diffuse terms.
- Image-based lighting (IBL):
  - Precompute irradiance (convolved diffuse) and specular prefiltered environment maps from an HDR cubemap.
  - Use a BRDF integration LUT (2D) to accelerate the specular integral for split-sum approximation.
- Implementation notes:
  - Shader stages compute direct lighting for dynamic lights and sample prefiltered maps for ambient/specular contribution.

### Physics (Jolt integration, rigidbody creation)

- Jolt is integrated as the physics backend:
  - The engine creates a Jolt Physics world and advances it each simulation tick.
  - Rigid bodies:
    - Created from engine entities with a Rigidbody component. Component contains mass, collision shape description and flags.
    - Collision shapes: built from primitive shapes (box, sphere, capsule) or from mesh-based colliders generated from model geometry (convex decomposition or triangle mesh for static geometry).
  - Synchronization:
    - Physics updates set transforms for entities with rigidbodies after stepping the simulation.
    - Kinematic bodies update physics state from the transform set by the engine.
- Collision callbacks/events:
  - The physics layer exposes collision and trigger events to higher-level systems.

### CMake setup

- The project uses CMake to configure builds and fetch/locate dependencies:
  - Top-level CMakeLists defines project, options, and platform-specific configuration (e.g., require Windows for DirectX11).
  - External libraries are added with one of:
    - find_package() for system-installed packages (SDL2, DirectX).
    - FetchContent or git submodules for third-party sources (imgui, stb, assimp, jolt).
  - CMake targets are split into engine core, tools (editor), and demo executable targets.
  - Typical CMake options:
    - DX11GAMEENGINE_BUILD_DEMO=ON/OFF
    - DX11GAMEENGINE_USE_ASSIMP=ON/OFF
    - DX11GAMEENGINE_BUILD_TESTS=ON/OFF
    - BUILD_SHARED_LIBS=ON/OFF

- Installation targets can copy runtime DLLs, shaders and asset folders to an output directory for running the demo.

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

*Ensure this file exists: `C:\vcpkg\scripts\buildsystems\vcpkg.cmake*`

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
*⚠️ The first configure may take several minutes.*

### 5. Build and Run

1. In the Visual Studio toolbar, select:
* **Configuration**: `Debug` or `Release`
* **Architecture**: `x64`
* **Startup target**: `DX11GameEngine`
2. **Build**: Build → Build All.
3. **Run**: Debug → Start Without Debugging (`Ctrl + F5`).
---

## Demo and Gallery

---
