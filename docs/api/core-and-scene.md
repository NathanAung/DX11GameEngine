# Core, Scene, Systems & Serialization API

This page documents the foundational identifier, math utility, scene container, ECS system, and serialization interfaces declared in `include/Engine/Core.h`, `UUID.h`, `MathUtils.h`, `Scene.h`, `Systems.h`, and `SceneSerializer.h`.

---

## Precompiled Core Header (`include/Engine/Core.h`)

`Core.h` is configured as the precompiled header (`target_precompile_headers`) for both `DX11Editor` and `DX11Runtime`. It includes:

- C++17 standard headers (`<cstdio>`, `<stdexcept>`, `<string>`, `<vector>`, `<cstdint>`)
- SDL2 (`<SDL.h>`, `<SDL_syswm.h>`)
- DirectX 11 (`<d3d11.h>`, `<dxgi.h>`, `<d3dcompiler.h>`, `<wrl/client.h>`, `<DirectXMath.h>`) and `#pragma comment(lib, ...)` directives for `d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`, and `dxguid.lib`
- Jolt Physics integration (`"Engine/PhysicsManager.h"`)

---

## `Engine::UUID` (`include/Engine/UUID.h`)

64-bit random identifier used to uniquely track assets across sessions, JSON scene files, and VFS archives. Specializes `std::hash<Engine::UUID>` for direct use as a key in `std::unordered_map`.

```cpp
namespace Engine
{
    class UUID
    {
    public:
        UUID();                       // Generates a random 64-bit ID via std::mt19937_64
        UUID(uint64_t uuid);          // Constructs from an existing 64-bit integer (0 = invalid/none)
        UUID(const UUID&) = default;

        operator uint64_t() const;    // Implicit conversion to uint64_t
    };
}
```

---

## `Engine::Math` (`include/Engine/MathUtils.h`)

Inline math helpers built on `DirectXMath` and `DirectXCollision` (left-handed coordinate system: `+X` right, `+Y` up, `+Z` forward, NDC depth `[0, 1]`).

### `Engine::Math::Ray`

```cpp
struct Ray
{
    DirectX::XMFLOAT3 origin;
    DirectX::XMFLOAT3 direction;
};
```

### Functions

| Signature | Description |
| :--- | :--- |
| `DirectX::XMFLOAT3 QuaternionToEulerDegrees(const DirectX::XMFLOAT4& q)` | Normalizes `q`, builds a rotation matrix, and extracts `(pitch, yaw, roll)` in degrees with `[-1, 1]` `asinf` clamping and ±90° gimbal-lock handling. |
| `DirectX::XMFLOAT4 EulerDegreesToQuaternion(const DirectX::XMFLOAT3& euler)` | Converts `(pitch, yaw, roll)` degrees to radians and returns a normalized quaternion via `XMQuaternionRotationRollPitchYaw`. |
| `Ray ScreenToWorldRay(float mouseX, float mouseY, float screenW, float screenH, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)` | Unprojects `(mouseX, mouseY)` at NDC `z = 0.0f` and `z = 1.0f` (`XMVector3Unproject`) to construct a normalized world-space picking ray. |
| `bool RayIntersectsOBB(const Ray& ray, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& rotation, float& outDistance)` | Tests intersection against a unit `DirectX::BoundingOrientedBox` centered at `position` with half-extents `0.5f * scale` and orientation `rotation`. Writes hit distance to `outDistance`. |
| `std::vector<DirectX::XMVECTOR> GetFrustumCornersWorldSpace(const DirectX::XMMATRIX& viewProj)` | Transforms the 8 DirectX NDC corners (`x, y in {-1, 1}`, `z in {0, 1}`) by `Inverse(viewProj)` with perspective divide; used by CSM cascade fitting. |

---

## `Engine::Scene` (`include/Engine/Scene.h`)

Central container managing the EnTT entity registry, play-mode backup registry, embedded `sol::state` Lua VM, entity factories, intrusive hierarchy links, and deferred destruction queue.

### Public Fields

| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `registry` | `entt::registry` | — | Primary live ECS registry. Exposed publicly by design for `view<>` queries and component access. |
| `m_backupRegistry` | `entt::registry` | — | Snapshot registry used to preserve pre-play state during `Edit <-> Play` transitions and `Game.Restart()`. |
| `m_activeRenderCamera` | `entt::entity` | `entt::null` | Entity handle of the camera currently driving rendering and 3D audio listener orientation. |
| `m_lua` | `sol::state` | — | Master Lua virtual machine state shared by sandboxed `ScriptInstance` environments. |
| `m_requestQuit` | `bool` | `false` | Set by `Game.Quit()`; instructs `main()` to exit at the end of the current frame. |
| `m_requestRestart` | `bool` | `false` | Set by `Game.Restart()`; instructs `main()` to restore `m_backupRegistry` and reinitialize scripts after rendering. |
| `m_assetManager` | `Engine::AssetManager*` | `nullptr` | Pointer to the engine's asset registry for runtime imports and VFS reads. |
| `m_currentScenePath` | `std::string` | `""` | Filepath of the currently loaded/saved JSON scene (empty if unsaved). |
| `m_entitiesToDestroy` | `std::vector<std::pair<entt::entity, float>>` | `{}` | Deferred destruction queue storing `(entity, timeRemainingSeconds)`. |

### Dependency & Path Accessors

| Method | Description |
| :--- | :--- |
| `Engine::PhysicsManager* GetPhysicsManager() const` | Returns the `PhysicsManager*` wired during `InitializeLuaBindings`. |
| `void SetAudioManager(Engine::AudioManager* am)` / `GetAudioManager() const` | Wires or retrieves `AudioManager*` for sound handle cleanup. |
| `void SetAssetManager(Engine::AssetManager* am)` / `GetAssetManager() const` | Wires or retrieves `AssetManager*` for asset lookups. |
| `void SetCurrentScenePath(const std::string& path)` / `GetCurrentScenePath() const` | Sets or gets the active `.json` scene filepath. |
| `void SetDefaultAssets(UUID shaderID, UUID debugShaderID, UUID skyboxShaderID, UUID spriteShaderID, UUID cubeID, UUID sphereID, UUID capsuleID, UUID quadMeshID)` | Caches core shader and primitive mesh UUIDs on the scene (`GetDefaultShaderID()`, `GetDebugShaderID()`, `GetSkyboxShaderID()`, `GetSpriteShaderID()`, `GetCubeMeshID()`, `GetSphereMeshID()`, `GetCapsuleMeshID()`, `GetQuadMeshID()`). |

### Entity Creation & Query Methods

| Signature | Description |
| :--- | :--- |
| `entt::entity CreateEntity(const std::string& name)` | Creates an entity with an auto-incremented `IDComponent`, `NameComponent{name}`, and identity `TransformComponent`. |
| `entt::entity CreateSampleEntity(const std::string& name)` | Creates an entity with a `MeshRendererComponent` positioned at `(0, 0, 5)` and scaled to `0.1`. |
| `entt::entity CreateCube(const std::string& name)` | Creates an entity with `MeshRendererComponent` bound to `m_cubeMeshID`. |
| `entt::entity CreateSphere(const std::string& name)` | Creates an entity with `MeshRendererComponent` bound to `m_sphereMeshID`. |
| `entt::entity CreateCapsule(const std::string& name)` | Creates an entity with `MeshRendererComponent` bound to `m_capsuleMeshID`. |
| `entt::entity CreateEditorCamera(const std::string& name, unsigned width, unsigned height)` | Creates a camera at `(0, 0, -10)` with `CameraComponent`, `ViewportComponent{width, height}`, and `EditorCamControlComponent`. Assigns `m_activeRenderCamera` if currently `entt::null`. |
| `entt::entity CreateGameCamera(const std::string& name, unsigned width, unsigned height)` | Creates an in-game camera at `(0, 0, -10)` with `CameraComponent` and `ViewportComponent{width, height}` (without `EditorCamControlComponent`). |
| `entt::entity CreateDirectionalLight(const char* name)` | Creates a white `LightType::Directional` light (`intensity = 5.0f`) pitched down 45° around `+X`. |
| `entt::entity CreatePointLight(const char* name, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& color, float intensity, float range)` | Creates a `LightType::Point` entity at `position` with `color`, `intensity`, and `range`. |
| `entt::entity CreateSpotLight(const char* name, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& color, float intensity, float range, float spotAngleRadians)` | Creates a `LightType::Spot` entity at `position`, building a quaternion aligning `+Z` with `direction`. |
| `entt::entity InstantiateModel(const std::string& filepath, Engine::Renderer& renderer, Engine::MeshManager& meshManager)` | Loads `filepath` via `MeshManager::LoadModel`, creates a parent entity named after the file stem, and parents one child entity (`<stem>_mesh_<i>`) per sub-mesh. |
| `entt::entity FindEntityByName(const std::string& name)` | Linearly searches `NameComponent` entries and returns the first matching `entt::entity`, or `entt::null`. |
| `entt::entity CastRay(const Engine::Math::Ray& ray, Engine::PhysicsManager& physicsManager)` | Queries Jolt narrow-phase raycasting first; if nothing is hit, falls back to `Math::RayIntersectsOBB` against active non-rigidbody entities (excluding `m_activeRenderCamera`). |

### Hierarchy, Lifecycle & Play-State Methods

| Signature | Description |
| :--- | :--- |
| `void ParentEntity(entt::entity child, entt::entity parent)` | Performs ancestor cycle detection, detaches `child` from any prior parent, converts its world transform into local space relative to `parent`, and appends it to `parent`'s intrusive child list. |
| `void UnparentEntity(entt::entity child)` | Removes `child` from its parent's sibling list and decomposes its cached `worldMatrix` into local TRS so its world transform is preserved. |
| `void DestroyEntity(entt::entity entity, Engine::PhysicsManager& physicsManager)` | Recursively destroys child entities, unparents `entity`, removes its Jolt rigid body, invokes Lua `OnDestroy()` callbacks, and destroys the EnTT handle. |
| `void SubmitForDestruction(entt::entity entity, float delay = 0.0f)` | Enqueues `(entity, delay)` in `m_entitiesToDestroy` (ignoring duplicates) for safe end-of-step deletion. |
| `void ProcessDestructionQueue(Engine::PhysicsManager& physicsManager, float deltaTime)` | Decrements queued timers and calls `DestroyEntity` on entries whose remaining time reaches `<= 0.0f`. |
| `void CopyToBackup()` | Clones all entities (preserving exact `entt::entity` IDs) and components from `registry` into `m_backupRegistry`. |
| `void RestoreFromBackup(Engine::PhysicsManager& physicsManager)` | Destroys active Jolt bodies and audio handles, clears `registry`, and restores all entities/components from `m_backupRegistry` with invalidated runtime handles. |
| `void StartPlayMode()` | Calls `CopyToBackup()`, runs `ScriptSystemInit(*this)`, and switches `m_activeRenderCamera` to the first active non-editor camera. |
| `void StopPlayMode(entt::entity editorCameraFallback, Engine::PhysicsManager& physicsManager)` | Shuts down Lua scripts, calls `RestoreFromBackup(physicsManager)`, and restores `m_activeRenderCamera = editorCameraFallback`. |
| `void InitializeLuaBindings(Engine::InputManager* inputManager, Engine::PhysicsManager* physicsManager)` | Opens standard Lua libraries and registers all engine usertypes, enums, and global tables into `m_lua`. |
| `void Clear(Engine::PhysicsManager& physicsManager)` | Destroys all Jolt bodies and audio handles, clears `registry` and `m_entitiesToDestroy`, and resets `m_activeRenderCamera` and `m_currentScenePath`. |
| `void GenerateDefaultSetup(Engine::PhysicsManager& physicsManager)` | Clears the scene and spawns `Editor Camera`, `Main Camera`, `Sun Light`, and a static `20 x 0.1 x 20` `Ground` box at `y = -5`. |

---

## `Engine::SceneSerializer` (`include/Engine/SceneSerializer.h`)

Static JSON scene saver and 4-pass loader powered by RapidJSON.

| Static Method | Description |
| :--- | :--- |
| `static bool Serialize(const std::string& filepath, Scene& scene)` | Writes all entities with an `IDComponent` to `filepath` under a top-level `"Scene"` JSON array, registers any attached Lua scripts and the scene itself in `AssetManager`, and saves `enginefiles/AssetRegistry.json`. |
| `static bool Deserialize(const std::string& filepath, Scene& scene, PhysicsManager& physicsManager, AssetManager& assetManager, std::string& outErrorMsg)` | Reads `filepath` from VFS (if active) or disk, validates all referenced asset UUIDs and script files *before* mutating the current world, clears the scene, reconstructs entities/components, and restores `RelationshipComponent` hierarchy links. Populates `outErrorMsg` on failure. |

---

## ECS Systems (`include/Engine/Systems.h`)

Stateless update and rendering passes operating on `Engine::Scene`:

### Simulation & Transform Systems

| Function Signature | Description |
| :--- | :--- |
| `void PhysicsSystem(Scene& scene, PhysicsManager& physicsManager, const MeshManager& meshManager, float dt, bool isPlaying)` | Creates/removes Jolt bodies based on active flags. When `isPlaying` is `true`, steps Jolt (`dt`) and pulls world transforms back into local `TransformComponent`s. When `false`, pushes ECS world matrices into Jolt and resets momentum. |
| `void ScriptSystemInit(Scene& scene)` | Creates isolated `sol::environment` instances (`self` bound), loads `.lua` files from VFS or disk, caches callbacks, and invokes `OnCreate()`. |
| `void ScriptSystemUpdate(Scene& scene, float dt)` | Updates `Time.deltaTime = dt`, lazily initializes newly attached runtime scripts (`OnCreate`), and invokes `OnUpdate(dt)` via `sol::protected_function`. |
| `void ScriptSystemShutdown(Scene& scene)` | Invokes `OnDestroy()` on all initialized scripts and resets script environments and callback handles. |
| `void AudioSystem(Scene& scene, AudioManager& audioManager, AssetManager& assetManager)` | Syncs the 3D listener to `m_activeRenderCamera`, lazily loads `AudioComponent` handles, starts `playOnCreate` clips, stops audio on inactive entities, and updates 3D emitter positions and volumes. |
| `void EditorCameraInputSystem(Scene& scene, const InputManager& input, float dt, bool isSceneFocused)` | Updates `EditorCamControlComponent` yaw/pitch (clamped to ±89°) on RMB capture, scroll-wheel zoom when focused, and `WASD` + `LShift` movement. |
| `void TransformSystem(Scene& scene)` | Traverses root entities and recursively cascades `World = Local * ParentWorld` down `RelationshipComponent` sibling chains. |
| `void CameraMatrixSystem(Scene& scene, Renderer& renderer)` | Computes LH view (`Inverse(world)`) and perspective projection (`XMMatrixPerspectiveFovLH`) matrices from `m_activeRenderCamera` and uploads them to `Renderer`. |
| `void DemoRotationSystem(Scene& scene, entt::entity sampleEntity, float dt)` | Utility demo system that rotates `sampleEntity` continuously around the X and Y axes. |

### `Engine::RenderSystem` Functions

| Function Signature | Description |
| :--- | :--- |
| `void RenderSystem::DrawEntities(Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Renderer& renderer, TextureManager& textureManager, FontManager& fontManager)` | Executes the 3-cascade shadow depth pass (`ShadowVS`), restores the main render target, frustum-culls and sorts up to 16 lights, JIT-loads missing meshes/textures, and draws `UnlitColor`, `LitColor`, and `Textured` PBR entities. |
| `void RenderSystem::DrawDebugColliders(Scene& scene, Renderer& renderer, MeshManager& meshManager, ShaderManager& shaderManager, entt::entity selectedEntity)` | Renders a bright green unlit wireframe mesh matching `selectedEntity`'s `RigidBodyComponent` collider shape and scale. |
| `void RenderSystem::DrawUI(Scene& scene, Renderer& renderer, MeshManager& meshManager, ShaderManager& shaderManager, TextureManager& textureManager, FontManager& fontManager)` | Configures orthographic pixel projection and alpha blending to render all active `SpriteRendererComponent` and `TextRendererComponent` entities (JIT-baking `.ttf` font atlases as needed). |
