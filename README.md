# Step Engine

**English** | [日本語](README.ja.md)

<img src="https://i.imgur.com/8bdSI8v.jpeg"/>

## Overview

**Step Engine** is a custom 3D game engine built from scratch in **C++17** and **DirectX 11**. Designed as a practical, modular architecture for real-time rendering and gameplay development, it ships with both a full **dockable Editor** and a **Standalone Game Runtime**.

Core capabilities include a DirectX 11 forward PBR renderer with tangent-space normal mapping and 3-cascade shadow maps, an EnTT-based ECS with intrusive entity hierarchies, multi-threaded Jolt Physics integration, sandboxed Lua scripting via `sol2`, 3D spatial audio, 2D sprite/font HUD rendering, a UUID-driven asset pipeline with binary `.pak` Virtual File System (VFS), and one-click standalone game export.

---

## License

Please review the [LICENSE](LICENSE) file before viewing or using this repository. All rights reserved; this repository is provided for viewing purposes only.

---

## Release

Download the latest release of Step Engine here:
- [v1.0 Release](https://github.com/NathanAung/DX11GameEngine/releases/tag/v1.0-release)

---

## Documentation

Browse the Step Engine documentation here:
- [Step Engine Documentation](https://nathanaung.github.io/DX11GameEngine/)

---

## Tech Stack

### Languages
- **C++17** — Core engine, rendering, physics, editor, and runtime architecture
- **HLSL (Shader Model 5.0)** — Direct3D 11 vertex and pixel shaders
- **Lua** — Gameplay scripting and runtime entity behavior
- **CMake / Batch / PowerShell** — Build configuration, dependency resolution, and environment setup scripts

### Libraries & Tools
All third-party dependencies are managed via **CMake** and **vcpkg (manifest mode)** (`vcpkg.json`), with ImGuizmo vendored under `external/`:
- **DirectX 11 (D3D11 / DXGI / D3DCompiler)** — GPU rendering backend
- **SDL2** — Window management, OS event loop, high-resolution timer, and raw keyboard/mouse input
- **EnTT** — Entity Component System (sparse-set registry)
- **Jolt Physics** — Multi-threaded rigid-body dynamics, collision detection, and raycasting
- **Assimp** — 3D model importing (`.obj`, `.fbx`, `.gltf`, `.glb`) with custom VFS stream hooks
- **Dear ImGui (DX11 + SDL2 + Docking)** & **ImGuizmo** — Editor interface and 3D transform gizmos
- **Lua** & **sol2** — Embedded scripting runtime and C++/Lua bindings
- **RapidJSON** — Scene and asset registry JSON serialization/deserialization
- **miniaudio** — 2D and 3D spatial audio engine
- **stb (`stb_image`, `stb_truetype`)** — Texture loading and TrueType font atlas baking
- **CMake**, **vcpkg**, and **Visual Studio 2022 (MSVC)** — Build toolchain

---

## Architecture & Implementation Details

### Rendering Pipeline

`Renderer` encapsulates the DirectX 11 device, immediate context, double-buffered swap chain, depth-stencil buffer, constant buffers, and pipeline states.

- **Editor vs. Standalone Render Targets**:
  - In **Editor builds**, the 3D scene renders to an off-screen **Render-to-Texture framebuffer** (`RTV` + `DSV` + `SRV`) that resizes dynamically with the ImGui Scene panel, before compositing ImGui onto the swap chain back buffer.
  - In **Standalone builds**, the engine renders directly to the swap chain back buffer.
- **Frame Pass Order**:
  1. **Cascaded Shadow Pass** — Depth-only pass (`ShadowVS.hlsl`, pixel shader unbound) rendering active lit geometry into each slice of the 3-cascade shadow map array.
  2. **Main 3D Geometry Pass** — Restores the main framebuffer/back buffer, uploads camera, light, and shadow constant buffers, and draws entities using PBR or Unlit shaders.
  3. **Debug Collider Pass (Edit Mode only)** — Renders wireframe green collider meshes for the currently selected entity.
  4. **Skybox Pass** — Draws a cube mesh with translation stripped from the view matrix and depth testing against a 6-face cubemap so the sky fills only untouched background pixels.
  5. **2D UI / HUD Pass** — Switches to alpha blending, disables depth testing/writing, and projects screen-space pixels via an orthographic matrix to render 2D sprites and dynamic text.
- **Meshes, Textures, Shaders & Fonts**:
  - `MeshManager` manages 32-bit indexed D3D11 vertex/index buffers (`Position`, `Normal`, `TexCoord`, `Tangent`), generating procedural primitives (`Cube`, `Sphere`, `Capsule`, `Quad`) and importing external models via Assimp while caching CPU-side vertices/indices for physics colliders.
  - `TextureManager` loads 2D images and 6-face cubemaps via `stb_image` into D3D11 SRVs, and generates 1×1 fallback textures to prevent shader state leakage.
  - `ShaderManager` compiles HLSL shaders to bytecode in Editor mode (caching `.cso` files on disk for packing) and loads pre-compiled `.cso` binaries directly from the VFS in Standalone builds.
  - `FontManager` bakes `.ttf` fonts into 2048×2048 RGBA texture atlases (ASCII 32–126) via `stb_truetype`, encoding glyph coverage in the alpha channel so `SpritePS.hlsl` can tint text dynamically.

---

### PBR Lighting, Normal Mapping & Cascaded Shadow Maps

#### Physically Based Shading (`BasicPS.hlsl`)
Surface shading uses a forward **Cook-Torrance microfacet BRDF**:
- **Normal Distribution (`D`)**: GGX / Trowbridge-Reitz
- **Geometry Term (`G`)**: Smith's method with Schlick-GGX
- **Fresnel (`F`)**: Fresnel-Schlick approximation with metallic workflow base reflectivity `F0 = lerp(0.04, albedo, metallic)`
- **Energy Conservation**: `kD = (1 - F) * (1 - metallic)` paired with a Lambertian diffuse term (`kD * albedo / PI`) and AO-modulated ambient term.

#### PBR Materials & Normal Mapping
- `MeshRendererComponent` supports three material modes: `LitColor`, `UnlitColor`, and `Textured`.
- Textured PBR materials bind three dedicated texture slots alongside scalar multipliers (`baseColor`, `roughness`, `metallic`):
  - `t0`: **Albedo Map**
  - `t1`: **Tangent-Space Normal Map**
  - `t2`: **ORM Map** (Red = Ambient Occlusion, Green = Roughness, Blue = Metallic)
- **Tangent-Space Normal Mapping**: `BasicVS.hlsl` transforms vertex normals and tangents into world space. In `BasicPS.hlsl`, the tangent is re-orthogonalized against the normal via **Gram-Schmidt orthogonalization** (`T = normalize(T - dot(T, N) * N)`), the bitangent is computed via `cross(N, T)`, and the sampled normal is transformed from tangent space to world space through the resulting `TBN` matrix.

#### Multi-Light Culling & Sorting
- Supports **Directional**, **Point**, and **Spot** lights (`LightComponent`), with up to **16 active lights** (`MAX_LIGHTS`) uploaded per frame in `CB_Light` (`b3`).
- Point and spot lights use a combined inverse-square and soft range-cutoff attenuation curve, plus cone angle checks for spot lights.
- Before uploading to the GPU, `RenderSystem::DrawEntities` builds a world-space `DirectX::BoundingFrustum` from the active camera, frustum-culls off-screen point/spot lights by their range sphere, prioritizes directional lights, and sorts remaining lights by squared distance to the camera.

#### Cascaded Shadow Maps (CSM)
- **Shadow Map Array**: Uses a 3-slice **2048×2048 `Texture2DArray`** with individual `D32_FLOAT` Depth-Stencil Views per cascade for depth rendering and a unified `R32_FLOAT` Shader Resource View (`t3`) for sampling.
- **Frustum Splitting & Orthographic Fitting**: `Renderer::CalculateCascadeMatrices` partitions the camera view frustum at **5%**, **20%**, and **100%** of the clip range (`near` to `far`), extracts each slice's 8 world-space frustum corners, encloses them in a bounding sphere, and constructs a tight orthographic light projection with extended Z-padding (`500.0f`) to capture shadow casters behind the camera.
- **Anti-Shimmering & Filtering**:
  - **Texel Snapping** rounds the light-space origin to shadow texel increments to eliminate shadow-edge swimming during camera movement.
  - **Hardware Slope-Scaled Depth Bias** (`SlopeScaledDepthBias = 1.5f`) combined with a minimal shader bias (`0.0001f`) mitigates shadow acne.
  - **3×3 Percentage-Closer Filtering (PCF)** samples the selected cascade slice (chosen from camera-space `viewZ` against `cascadeSplits`) to produce softer shadow edges on directional lights.

---

### Entity Component System (ECS)

The scene world is powered by **EnTT** (`Scene::registry`), storing lightweight POD components in cache-friendly sparse sets.

#### Core Components
| Component | Purpose |
| :--- | :--- |
| `IDComponent` / `NameComponent` | Stable 64-bit serialization ID, display name, and master entity `isActive` toggle |
| `TransformComponent` | Local `position`, quaternion `rotation`, `scale`, cached `worldMatrix`, and `isDirty` flag |
| `RelationshipComponent` | Intrusive hierarchy links (`parent`, `firstChild`, `prevSibling`, `nextSibling`) |
| `MeshRendererComponent` | Mesh UUID, Albedo/Normal/ORM texture UUIDs, `MaterialType`, color tint, roughness, metallic |
| `LightComponent` | Directional/Point/Spot light type, color, intensity, range, and cone angle |
| `CameraComponent` / `ViewportComponent` / `EditorCamControlComponent` | Camera FOV/clip planes, viewport dimensions, and editor fly-camera parameters |
| `RigidBodyComponent` | Collider shape/dimensions, static/dynamic motion, mass, friction, restitution, damping, Jolt `BodyID` |
| `LuaScriptComponent` | List of attached `ScriptInstance` entries (isolated `sol::environment` + cached callbacks) |
| `AudioComponent` | Audio asset UUID, 2D/3D spatial toggle, loop/play-on-create flags, volume, runtime handle |
| `SpriteRendererComponent` / `TextRendererComponent` | Screen-space 2D sprite quads (position, size, pivot, rotation, tint) and TTF text elements |

#### Entity Hierarchy & Transform Propagation
- **Intrusive Doubly-Linked Tree**: `RelationshipComponent` models arbitrary parent-child hierarchies without heap-allocating per-entity child vectors.
- **Safe Reparenting**: `Scene::ParentEntity` performs ancestor traversal for **cycle detection**, detaches from any previous parent, and preserves the entity's world transform by computing `newLocal = childWorld * Inverse(parentWorld)` before decomposing back into local TRS. `Scene::UnparentEntity` promotes the world matrix back to local TRS.
- **Hierarchical Updates**: `TransformSystem` starts at root entities (`parent == entt::null`) and recursively cascades `World = Local * ParentWorld` down the sibling chains, propagating dirty flags whenever an ancestor moves.
- **Cascade & Deferred Destruction**: `Scene::DestroyEntity` recursively destroys descendants, unregisters Jolt physics bodies, invokes Lua `OnDestroy` callbacks, and repairs sibling links. During gameplay, `Scene::SubmitForDestruction(entity, delay)` queues timed removals processed safely at the end of the update step to prevent iterator invalidation.

#### Frame Update Order
Each frame runs systems in a deterministic sequence:
1. `PhysicsSystem` (body creation/cleanup + Jolt simulation in Play mode or gizmo sync in Edit mode)
2. `ScriptSystemUpdate` *(Play mode)*
3. `AudioSystem` *(Play mode)*
4. `Scene::ProcessDestructionQueue` *(Play mode)*
5. `EditorCameraInputSystem` *(Edit mode)*
6. `TransformSystem` (local-to-world hierarchy cascade)
7. `CameraMatrixSystem` (active camera view/projection upload)

---

### Physics

`PhysicsManager` integrates **Jolt Physics** with a multi-threaded `JobSystemThreadPool` (`hardware_concurrency - 1` worker threads), a 128 MB `TempAllocatorImpl`, and capacity for up to 10,240 bodies and 65,536 contact constraints.

- **Broadphase & Collision Layers**: Separates static and dynamic layers so static bodies collide only with moving bodies while dynamic bodies collide with both.
- **Collision Shapes & Caching**:
  - Supports **Box**, **Sphere**, **Capsule**, and **Mesh** colliders.
  - Mesh colliders automatically generate indexed concave `JPH::MeshShape` geometry for static bodies and `JPH::ConvexHullShape` geometry for dynamic bodies, caching built shapes by mesh UUID (`m_meshShapeCache`).
  - Entity world scale (combined with `colliderScale` for mesh colliders) is applied via `JPH::ScaledShape`.
- **Bidirectional ECS Synchronization**:
  - **Play Mode**: Steps Jolt with clamped delta time (`<= 1/30 s`) and 4 collision sub-steps to reduce tunneling, then pulls Jolt world-space transforms back into ECS local transforms (converting via the inverse parent world matrix—and querying Jolt directly if the parent is also a physics body to avoid 1-frame lag).
  - **Edit Mode**: Pushes Inspector and ImGuizmo world transforms directly into Jolt, teleporting bodies, zeroing momentum, and rebuilding bodies automatically when scale, shape, mass, or damping changes.
- **Raycasting**: `Scene::CastRay` queries Jolt's narrow-phase raycast first and falls back to mathematical Oriented Bounding Box (OBB) intersection for non-physics entities such as lights or uncollided meshes.
- **Thread-Safe Collision Callbacks**: `PhysicsContactListener` captures `OnContactAdded` and `OnContactRemoved` events from Jolt worker threads into a mutex-protected queue and flushes them sequentially on the main thread after `Update()`, routing `OnCollisionEnter(other)` and `OnCollisionExit(other)` directly into attached Lua scripts.

---

### Engine Editor

The editor is built with **Dear ImGui (Docking)** and **ImGuizmo**, excluded automatically from standalone game builds via the `DIST_BUILD` preprocessor macro.

#### Dockable Panels & Workflows
- **Toolbar**: Houses the **Play / Stop** toggle and the **Export Game** button.
- **Scene Viewport**: Displays the off-screen framebuffer SRV, dynamically resizing the active camera viewport and framebuffer. Supports:
  - **ImGuizmo 3D Manipulation**: Translate, Rotate, and Scale handles (cycle with `Space`), automatically transforming manipulated world matrices back into local space for parented entities.
  - **Object Picking**: Left-clicking casts a screen-to-world ray through `Scene::CastRay`.
  - **Fly Camera Navigation**: Right-click captures the cursor in relative mouse mode for WASD + Shift sprint + mouse-look navigation, plus scroll-wheel zooming, with smart input routing between ImGui and `InputManager`.
- **Hierarchy**: Displays the entity tree with right-click creation (empty entities, cameras, primitives, lights), deletion (`Delete` key or context menu), drag-and-drop reparenting/unparenting, and dropping 3D models directly into the scene (`Scene::InstantiateModel` automatically creates a parent entity with child sub-mesh entities).
- **Inspector**: Live editing of all attached components with active toggles, component add/remove menus, Euler-cached quaternion rotation editing (preventing gimbal jitter), real-time physics property syncing, and drag-and-drop targets for PBR textures, Lua scripts, audio clips, and `.ttf` fonts.
- **Content Browser**: Navigates `assets/`, loads `.json` scenes on click (with VRAM preloading), provides a right-click **Create New Scene** workflow, and acts as a drag-and-drop source for models, textures, scripts, and audio. Supports `Ctrl+S` quick-save and modal dialogs for saving, creating scenes, and error reporting.

#### Editor State Machine (`Edit` <-> `Play`)
`EditorUI` manages two states (`Edit` and `Play`) to allow non-destructive playtesting directly inside the editor:
- **Entering Play Mode (`Edit -> Play`)**:
  1. `Scene::CopyToBackup()` snapshots all entities and components (preserving exact EnTT entity handles) into `m_backupRegistry`.
  2. `ScriptSystemInit` initializes all attached Lua environments and invokes `OnCreate()`.
  3. The active render camera switches from the cached Editor Camera to the first active Game Camera.
  4. Editor panels (Hierarchy, Inspector, Content Browser) hide so the viewport acts as the game view, and `Update()` enables physics simulation, Lua updates, and audio.
- **Exiting Play Mode (`Play -> Edit`)**:
  1. `ScriptSystemShutdown` invokes `OnDestroy()` and resets all script instances.
  2. `Scene::RestoreFromBackup()` tears down all runtime Jolt bodies and `miniaudio` sound handles, clears the live registry, and restores the clean pre-play snapshot (invalidating runtime body/audio handles so subsystems reinitialize cleanly).
  3. The viewport reverts to the cached Editor Camera and all editor panels reappear.

---

### Lua Scripting System

Gameplay logic is written in **Lua** and integrated via **sol2**.

- **Per-Script Sandboxing & Multi-Script Support**: `LuaScriptComponent` holds a vector of `ScriptInstance` objects, allowing multiple scripts per entity. Each script executes inside its own isolated `sol::environment` with a `self` variable bound to a `ScriptEntity` wrapper, preventing global namespace collisions between entities.

- **Lifecycle & Physics Callbacks**:
  - `OnCreate()` — Called when Play Mode starts (or on-the-fly on the first frame a script is attached at runtime).
  - `OnUpdate(dt)` — Called every frame during Play Mode with delta time.
  - `OnDestroy()` — Called when the entity is destroyed or Play Mode exits.
  - `OnCollisionEnter(other)` / `OnCollisionExit(other)` — Invoked on the main thread from Jolt physics contact events with the colliding `ScriptEntity`.

- **Stability & Error Isolation**: Scripts are loaded with `safe_script` / `safe_script_file` and invoked via `sol::protected_function` so syntax or runtime errors log to `stderr` without crashing the engine. Iteration snapshots entity lists and copies callbacks to the C++ stack to remain safe against EnTT pool reallocations if scripts instantiate entities mid-frame.

- **Exposed Scripting API**:
  - **`Entity` (`self`)**: 
    - Transform access (`GetTransform`, `GetForwardVector`, `GetRightVector`)
    - Activation (`IsActive`, `SetActive`)
    - Hierarchy (`SetParent`)
    - Spawning & deferred destruction (`Instantiate`, `InstantiateCube`, `InstantiateSphere`, `Destroy(delay)`)
    - Runtime component checks & attachment (`AddMeshRenderer`, `AddRigidBody`, `AddLuaScript`, `AddAudioComponent`, `AddSpriteRenderer`, `AddTextRenderer`)
    - PBR material/mesh assignment (`SetMesh`, `SetAlbedo`, `SetNormal`, `SetORM`)
    - Lighting (`SetLightColor`, `SetLightIntensity`), 
    - Physics (`ApplyLinearImpulse`, `SetLinearVelocity`, `GetLinearVelocity`)
    - Audio (`PlayAudio`, `StopAudio`, `SetAudioVolume`)
    - 2D HUD (`SetSprite`, `SetSpriteTint`, `SetText`, `SetTextColor`, `SetTextPosition`, `SetFont`)
  - **Global Tables**: 
    - `Input` (`IsKeyDown`, `IsKeyPressed`, `IsKeyReleased` + `Key` enum)
    - `Physics.CastRay(...)`
    - `Scene.FindEntityByName(name)`
    - `Time.deltaTime`
    - `Game.Restart()` / `Game.Quit()`

```lua
-- Example: Spawning a physics sphere with audio on keypress
function OnUpdate(dt)
    if Input:IsKeyPressed(Key.Space) then
        local ball = self:Instantiate("Bouncing Ball")
        local pos = self:GetTransform().position
        ball:GetTransform():SetPosition(pos.x, pos.y + 2.0, pos.z)

        ball:AddMeshRenderer(MeshType.Sphere)
        ball:AddRigidBody(RBShape.Sphere, RBMotion.Dynamic, 0.5, 1.0, 0.8)
        ball:AddAudioComponent("assets/audio/spawn.mp3", true, false, true)
    end
end
```

---

### Asset Management, VFS & Game Export

#### UUIDs & Asset Registry
- Every asset (meshes, model files, textures, shaders, audio, fonts, Lua scripts, and scenes) is identified by a random 64-bit **`Engine::UUID`**.
- `AssetManager` maintains a central registry mapping `UUID -> AssetMetadata` (`filepath`, `AssetType`, `isLoaded`), deduplicating imports by path and supporting virtual/procedural identifiers.
- **Persistent Asset Ledger**: In Editor mode, the registry is serialized to and restored from `enginefiles/AssetRegistry.json` via RapidJSON so procedural and physical assets keep identical UUIDs across sessions.
- **Just-In-Time (JIT) Loading & Preloading**: `RenderSystem` lazily resolves and uploads missing meshes, PBR textures, and font atlases via UUID metadata on demand, while standalone builds preload registered models and textures into VRAM at startup (`PreloadGameAssets`) to avoid runtime hitches.

#### Scene Serialization & Deserialization
`SceneSerializer` saves and loads scenes as human-readable JSON using RapidJSON:
- **Serialization**: Writes all entity components using asset UUIDs and converts transient `entt::entity` relationship handles into stable `IDComponent` 64-bit IDs.
- **4-Pass Deserialization**:
  1. **Validation Pass**: Verifies every referenced mesh, PBR map, physics mesh, audio clip, font, and script exists in the registry (and on disk in Editor mode); aborts cleanly with an error modal before modifying the active scene if anything is missing.
  2. **Scene Clear**: Safely tears down current entities, Jolt bodies, and audio handles.
  3. **Entity Reconstruction**: Recreates entities and components while building a `savedID -> entt::entity` translation map.
  4. **Hierarchy Restoration**: Rebuilds `RelationshipComponent` parent/child/sibling links from the translation map.

#### Asset Packing & Virtual File System (VFS)
- **`PAK2` Binary Archive (`AssetManager::PackAssets`)**: Compiles all physical files tracked in the registry into a single `data.pak` archive structured as:
  - 4-byte magic header (`"PAK2"`) + `uint32_t` asset count
  - Dynamic **Table of Contents (TOC)** storing `UUID`, `AssetType`, filepath string, byte offset, and byte size per entry
  - Concatenated raw binary payloads
- **Transparent VFS Mounting (`MountVFS` / `ReadAssetFromVFS`)**: In Standalone builds, `DX11Runtime` mounts `data.pak` at boot, reconstructing both the asset registry and TOC offset table in memory. All engine loaders read seamlessly from VFS memory buffers without touching loose asset files:
  - **Models**: Custom `Assimp::IOSystem` (`VFSIOSystem`) and `Assimp::IOStream` (`VFSIOStream`)
  - **Textures**: `stbi_load_from_memory`
  - **Shaders**: Pre-compiled `.cso` bytecode
  - **Audio**: `miniaudio` memory decoder (`ma_decoder_init_memory`)
  - **Fonts**: In-memory `.ttf` buffer baked via `stbtt_BakeFontBitmap`
  - **Scripts & Scenes**: Extracted directly into strings for `sol::state::safe_script` and RapidJSON parsing

#### Standalone Game Export Pipeline
Clicking **Export Game** in the editor toolbar calls `AssetManager::ExportProject()`, which automates building a distributable game directory (`ExportedGame/`):
1. Serializes the active scene and registers any newly attached scripts.
2. Packs all project assets, compiled `.cso` shaders, scripts, and scenes into `ExportedGame/data.pak`.
3. Copies the pre-built `DX11Runtime.exe` and its required DLLs from `RuntimeTemplate/`.
4. Writes `ExportedGame/enginefiles/Launch.txt` targeting the active scene as the startup level.
5. Opens the `ExportedGame/` directory ready to run standalone.

---

### Audio & Input

- **Audio (`AudioManager` & `AudioSystem`)**: Wraps `miniaudio` to provide both 2D audio and 3D spatialized sound emitters (`AudioComponent`) with looping, volume control, and play-on-create behavior. Each frame in Play mode, `AudioSystem` synchronizes the listener's world position and forward vector with the active camera and updates 3D emitter positions from decomposed entity world matrices.
- **Window & Input (`SDL2` & `InputManager`)**: SDL2 manages the Windows `HWND` (passed to Direct3D 11 via `SDL_SysWMinfo`), high-resolution frame timing, window resize events, and input polling. `InputManager` tracks current and previous frame key states (`IsKeyDown`, `IsKeyPressed`, `IsKeyReleased`), mouse deltas, scroll wheel offsets, and relative mouse capture.

---

### CMake Configuration

The top-level [CMakeLists.txt](CMakeLists.txt) configures the project with **CMake 3.20+**, **C++17**, MSVC warning/conformance flags (`/permissive- /W4 /EHsc` with `/external:W0` for third-party headers), and shared precompiled headers.

- **Dual Executable Targets**:
  - **`DX11Editor`** *(Default Visual Studio startup project)* — Full engine with Dear ImGui, ImGuizmo, and disk-based asset authoring.
  - **`DX11Runtime`** — Compiled with the `DIST_BUILD` macro to strip editor UI and boot directly into `data.pak` VFS + Play mode.
- **Dependencies**: Resolves `SDL2`, `assimp`, `EnTT`, `RapidJSON`, `imgui`, `Jolt`, `lua`, `sol2`, `stb`, and `miniaudio` via vcpkg manifest mode, and links Windows DirectX libraries (`d3d11`, `dxgi`, `d3dcompiler`, `dxguid`).
- **Asset Copy & Runtime Template Packaging**:
  - Custom build targets (`CopyShaders`, `CopyAssets`, `CopyEngineFiles`) mirror `shaders/`, `assets/`, and `enginefiles/` into the runtime output directory.
  - A `POST_BUILD` step on `DX11Runtime` automatically assembles `RuntimeTemplate/` inside `bin/` with `DX11Runtime.exe` and all required `.dll` files so the Editor's **Export Game** feature can package standalone builds immediately.

---

## Project Setup (Visual Studio)

This project uses **CMake + vcpkg (manifest mode)**, so all third-party dependencies are automatically downloaded and built during CMake configuration.

### Prerequisites

- **Windows 10 / 11** (with DirectX 11 runtime)
- **Visual Studio 2022**
  - **Workload**: Desktop development with C++
  - **Components**: MSVC, C++ CMake tools for Windows, Git

### 1. Clone the Repository

```bash
git clone https://github.com/NathanAung/DX11GameEngine.git
```

### 2. Install CMake and vcpkg (Automated Setup)

If **CMake** and **vcpkg** are not installed on your machine yet, run the setup script located in the `tools` folder:

```bat
tools\SetupProject.bat
```

This script automatically:
- Downloads and installs **CMake** into your local AppData directory (`%LOCALAPPDATA%\w64cmake`).
- Clones and bootstraps **vcpkg** on your system drive (e.g., `C:\vcpkg`).

Verify that `%SystemDrive%\vcpkg\scripts\buildsystems\vcpkg.cmake` exists before proceeding.

> Project setup tool provided by Francesco Paolo Mariani.

### 3. Open the Project in Visual Studio

1. Launch **Visual Studio 2022**.
2. Select **File → Open → Folder...** and choose the cloned `DX11GameEngine` directory.
3. Visual Studio will automatically detect `CMakeLists.txt` and `CMakeSettings.json`.

### 4. Configure CMake Settings (Visual Studio UI)

1. Open **Project → CMake Settings for DX11GameEngine**.
2. Select a configuration (e.g., `x64-Debug` or `x64-Release`).
3. Ensure the **CMake toolchain file** (`CMAKE_TOOLCHAIN_FILE`) is set to:
   - `${env.SystemDrive}/vcpkg/scripts/buildsystems/vcpkg.cmake`
4. Save the settings. Visual Studio will configure CMake and invoke vcpkg to install all dependencies listed in `vcpkg.json`.
   - *Note: The first configuration may take several minutes while vcpkg builds the dependencies.*

### 5. Build and Run

1. In the Visual Studio toolbar:
   - **Configuration**: `x64-Debug` or `x64-Release`
   - **Startup Item**: `DX11Editor.exe`
2. **Build**: Select **Build → Build All** (building both `DX11Editor` and `DX11Runtime` also populates `RuntimeTemplate/` required for the Editor's **Export Game** feature).
3. **Run**: Select **Debug → Start Without Debugging (`Ctrl + F5`)**.

---

## Archive

> **Note:** The portfolio, videos, and screenshots below are from earlier versions of Step Engine and do not reflect the latest features (such as the ImGui Editor, Cascaded Shadow Maps, Normal Mapping, Lua Scripting, Audio, 2D UI, VFS, and Game Export).

### Portfolio (Outdated)
For earlier architecture notes and development details, see the archived portfolio:
- [View Portfolio (Google Drive)](https://drive.google.com/file/d/1DbQy8O9z-OBZ13gIXDv29bzn-9GjP03G/view?usp=sharing)

### Development Update Video (2025/12/22)
[![Development Update](https://i.imgur.com/VwbFdgl.jpeg)](https://youtu.be/ANGTMzYf98M)

### Early Demo Videos (Click to Play)

#### Lighting Demo
[![Lighting Demo](https://i.imgur.com/0RIBnX6.jpeg)](https://youtu.be/dPvAgLWRbKk)

#### Galton Board Physics Demo
[![Galton Board Physics Demo](https://i.imgur.com/Bj7wcyO.jpeg)](https://youtu.be/JmqsDxMvVeI)

#### Wall Smasher Game Demo
[![Wall Smasher Game Demo](https://i.imgur.com/jCIpugV.jpeg)](https://youtu.be/DlM2aOIT1i8)

#### 3D Model Demo
[![3D Model Demo](https://i.imgur.com/4AC6h1Z.jpeg)](https://youtu.be/XPVFZV_9ea4)

### Early Gallery

#### PBR Lighting
- Metallic / Roughness PBR materials
<img src="https://i.imgur.com/FqLpL3O.jpeg"/>

- Directional, Spot, and Point lights
<img src="https://i.imgur.com/fT8hAZw.jpeg"/>

#### 3D Model Loading & Rendering
<img src="https://i.imgur.com/qVgbAAt.jpeg"/>

<img src="https://i.imgur.com/nliMOQQ.jpeg"/>
