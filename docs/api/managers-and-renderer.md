# Renderer, Managers & Editor UI API

This page documents the DirectX 11 renderer, resource/asset managers, physics/audio/input subsystems, and Dear ImGui editor classes declared across `include/Engine/`.

---

## `Engine::Renderer` (`include/Engine/Renderer.h`)

Encapsulates the Direct3D 11 device, immediate context, swap chain, constant buffers, pipeline state objects, off-screen Editor framebuffer, and Cascaded Shadow Map resources.

### GPU Data Structures

- **`DX11Context`**: Holds `ComPtr` handles for `device`, `context`, `swapChain`, `rtv`, `depthStencilBuffer`, `dsv`, `featureLevel` (default `D3D_FEATURE_LEVEL_11_0`), `width` (`1280`), and `height` (`720`).
- **`LightData`** *(64 bytes, 16-byte aligned)*: `position` (`XMFLOAT3`), `range` (`float`), `direction` (`XMFLOAT3`), `spotAngle` (`float`), `color` (`XMFLOAT3`), `intensity` (`float`), `type` (`unsigned int`: `0=Dir, 1=Point, 2=Spot`), `padding` (`XMFLOAT3`).
- **`LightConstants`** *(PS `b3`, `#define MAX_LIGHTS 16`)*: `cameraPos` (`XMFLOAT3`), `lightCount` (`unsigned int`), `lights[MAX_LIGHTS]` (`LightData`).
- **`MaterialConstants`** *(PS `b4`, 32 bytes)*: `baseColor` (`XMFLOAT4`), `roughness` (`float`), `metallic` (`float`), `padding[2]` (`float`).
- **`ShadowConstants`** *(PS `b5`)*: `cascadeViewProj[3]` (`XMFLOAT4X4`), `cascadeSplits` (`XMFLOAT4`: `x=near, y=mid, z=far`).
- **`Renderer::CascadeData`** (`static const int NUM_CASCADES = 3`): `viewProj` (`XMMATRIX`), `splitDepth` (`float`).

### Public Methods

| Category | Signature | Description |
| :--- | :--- | :--- |
| **Lifecycle** | `bool InitD3D11(HWND hwnd, unsigned width, unsigned height)` | Initializes device, swap chain, RTV/DSV, pipeline states, CBs, CSM textures, and Editor framebuffer (`#ifndef DIST_BUILD`). |
| | `void Shutdown()` | Releases all `ComPtr` GPU states, buffers, views, swap chain, context, and device. |
| | `void Present(bool vsync)` | Presents the swap chain (`SyncInterval = vsync ? 1 : 0`). |
| | `bool Resize(unsigned width, unsigned height)` | Resizes swap chain buffers, main RTV/DSV, and Editor framebuffer (ignores `0x0` minimize). |
| **Render Targets** | `void BeginFrame()` | Binds and clears main RTV/DSV, sets viewport/states, and binds VS `b0`–`b2`. |
| | `bool CreateFramebuffer(UINT width, UINT height)` | Allocates/resizes the off-screen `R8G8B8A8_UNORM` texture (`RTV` + `SRV`) and `D24_UNORM_S8_UINT` depth target. |
| | `void BindFramebuffer()` | Binds and clears the off-screen Editor framebuffer and resets rasterizer/depth/VS CB states. |
| | `void BindBackBuffer()` | Binds and clears the main swap-chain back buffer to black. |
| | `ID3D11ShaderResourceView* GetFramebufferSRV() const` | Returns the off-screen scene SRV for `ImGui::Image`. |
| **Constants & Draws** | `void UpdateProjectionMatrix(const DirectX::XMMATRIX& proj)` | Updates VS `b0`. |
| | `void UpdateViewMatrix(const DirectX::XMMATRIX& view)` | Updates VS `b1`. |
| | `void UpdateWorldMatrix(const DirectX::XMMATRIX& world)` | Updates VS `b2`. |
| | `void UpdateLightConstants(const LightConstants& data)` | Uploads and binds PS `b3`. |
| | `void UpdateMaterialConstants(const MaterialConstants& material)` | Uploads and binds PS `b4`. |
| | `void UpdateColorConstants(const DirectX::XMFLOAT4& color)` | Dynamically maps (`WRITE_DISCARD`) and binds PS `b0` (`UnlitPS` / `SpritePS`). |
| | `void UpdateShadowConstants(const ShadowConstants& data)` | Uploads and binds PS `b5`. |
| | `void BindShader(const ShaderManager& shaderMan, UUID shaderID)` | Binds VS, PS, and input layout from `ShaderManager`. |
| | `void SubmitMesh(const MeshBuffers& mesh, ID3D11InputLayout* inputLayout)` | Binds input layout, vertex buffer, index buffer, and `TRIANGLELIST` topology. |
| | `void DrawIndexed(UINT indexCount)` | Issues `ID3D11DeviceContext::DrawIndexed(indexCount, 0, 0)`. |
| **Shadows (CSM)** | `void CalculateCascadeMatrices(const DirectX::XMMATRIX& camView, float camFov, float aspect, float camNear, float camFar, const DirectX::XMFLOAT3& lightDir)` | Computes texel-snapped light-space `viewProj` matrices and split depths for all 3 cascades (`5%`, `20%`, `100%`). |
| | `const std::vector<CascadeData>& GetCascades() const` | Returns computed cascade matrices and split depths. |
| | `void BindShadowPass(int cascadeIndex)` | Unbinds PS SRVs (`0..15`), binds `m_shadowDSVs[cascadeIndex]`, clears depth, sets `2048x2048` viewport, `m_shadowRasterState`, and `PS = nullptr`. |
| | `ID3D11ShaderResourceView* GetShadowSRV() const` | Returns the 3-slice `R32_FLOAT` `Texture2DArray` SRV (`t3`). |
| **Skybox & States** | `void SetSkybox(ID3D11ShaderResourceView* srv)` | Assigns the active cubemap SRV. |
| | `void DrawSkybox(const MeshManager& meshMan, const ShaderManager& shaderMan, const CameraComponent& camComp, const TransformComponent& camTrans, UUID cubeMeshID, UUID skyboxShaderID)` | Draws the cubemap skybox with translation-stripped view matrix and `LESS_EQUAL` depth state. |
| | `void SetWireframeMode(bool enable)` | Toggles between `m_wireframeRasterizerState` (`CULL_NONE`) and `m_solidRasterizerState`. |
| | `void SetUIBlendState(bool enable)` / `SetUIDepthState(bool enable)` | Toggles alpha blending and depth-disabled states for 2D UI rendering. |
| **Accessors** | `GetDevice()`, `GetContext()`, `GetRTV()`, `GetDSV()`, `GetRasterState()`, `GetDepthStencilState()`, `GetSamplerState()`, `GetLightCB()`, `GetWidth()`, `GetHeight()` | Low-level D3D11 object and dimension getters. |

---

## `Engine::AssetManager` (`include/Engine/AssetManager.h`)

Tracks asset UUIDs, persists `enginefiles/AssetRegistry.json`, packs/mounts `PAK2` VFS archives, and exports standalone builds.

### Types

```cpp
enum class AssetType
{
    None = 0, Mesh, Texture, Audio, Shader, ModelFile, LuaScript, Scene, Font
};

struct AssetMetadata
{
    UUID handle = 0;
    std::string filepath = "";
    AssetType type = AssetType::None;
    bool isLoaded = false;
};

struct AssetManager::TOCEntry
{
    uint64_t uuid;
    uint32_t type;
    uint64_t offset;
    uint64_t size;
};
```

### Public Methods

| Signature | Description |
| :--- | :--- |
| `UUID ImportAsset(const std::string& filepath, AssetType type)` | Validates file existence (on disk in Editor, or in registry when VFS is active; skips virtual `primitive://`, `shader://`, `cubemap://` URIs), deduplicates by `filepath`, and returns its `UUID` (`0` on failure). |
| `const AssetMetadata* GetMetadata(UUID handle) const` | Looks up metadata for `handle` (`nullptr` if unknown). |
| `void SetAssetLoaded(UUID handle, bool loaded)` | Updates `AssetMetadata::isLoaded`. |
| `const std::unordered_map<UUID, AssetMetadata>& GetRegistry() const` | Read-only access to the full asset map. |
| `bool SaveRegistry(const std::string& filepath)` | Writes all known assets to JSON (no-op returning `true` when VFS is active). |
| `bool LoadRegistry(const std::string& filepath)` | Restores UUID mappings from JSON with `isLoaded = false`. |
| `bool PackAssets(const std::string& pakFilepath) const` | Writes all registered entries and physical file payloads into a `PAK2` binary archive. |
| `bool MountVFS(const std::string& pakFilepath)` | Validates `"PAK2"` header, populates `m_assetRegistry` and `m_vfsTable`, and activates VFS mode (`IsVFSActive() == true`). |
| `std::vector<char> ReadAssetFromVFS(UUID handle) const` | Seeks to `TOCEntry::offset` in `m_vfsPakPath` and returns `size` bytes. |
| `bool IsInVFS(UUID handle) const` / `bool IsVFSActive() const` | Queries VFS archive membership and active mount state. |
| `bool ExportProject(Scene& activeScene, std::string& outErrorMsg) const` | Saves `activeScene`, packs `ExportedGame/data.pak`, copies `RuntimeTemplate/`, writes `ExportedGame/enginefiles/Launch.txt`, and opens Explorer. |

---

## Resource Managers

### `Engine::MeshManager` (`include/Engine/MeshManager.h`)

- **Structs**:
    - `Vertex`: `position` (`XMFLOAT3`), `normal` (`XMFLOAT3`), `texCoord` (`XMFLOAT2`), `tangent` (`XMFLOAT3`).
    - `MeshBuffers`: `vertexBuffer` (`ID3D11Buffer*`), `indexBuffer` (`ID3D11Buffer*`), `indexCount` (`UINT`), `stride` (`UINT`), `indexFormat` (`DXGI_FORMAT_R32_UINT`).
- **Methods**:
    - `UUID InitializeCube(ID3D11Device* device, AssetManager& assetManager)` — Unit cube (`primitive://cube`).
    - `UUID InitializeQuad(ID3D11Device* device, AssetManager& assetManager)` — `[0..1]` XY unit quad (`primitive://quad`) for 2D sprites/text.
    - `UUID CreateSphere(ID3D11Device* device, AssetManager& assetManager, float radius, int slices, int stacks)`
    - `UUID CreateCapsule(ID3D11Device* device, AssetManager& assetManager, float radius, float cylinderHeight, int slices, int stacks)`
    - `std::vector<UUID> LoadModel(ID3D11Device* device, AssetManager& assetManager, const std::string& filename)` — Imports via Assimp (`VFSIOSystem` enabled), caches results by `filename`, and returns sub-mesh UUIDs (`<filename>?mesh=<i>`).
    - `bool GetMesh(UUID meshID, MeshBuffers& out) const`
    - `const std::vector<DirectX::XMFLOAT3>& GetMeshPositions(UUID meshID) const` / `GetMeshIndices(UUID meshID) const` — CPU-cached geometry for Jolt `MeshShape` / `ConvexHullShape` generation.

### `Engine::ShaderManager` (`include/Engine/ShaderManager.h`)

- `UUID LoadBasicShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://basic` -> `BasicVS` + `BasicPS`)
- `UUID LoadSkyboxShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://skybox` -> `SkyboxVS` + `SkyboxPS`)
- `UUID LoadUnlitShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://unlit` -> `UnlitVS` + `UnlitPS`)
- `UUID LoadSpriteShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://sprite` -> `SpriteVS` + `SpritePS`)
- `UUID LoadShadowShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://shadow` -> `ShadowVS`, `ps = nullptr`)
- `void Bind(UUID shaderID, ID3D11DeviceContext* context) const`
- `ID3D11InputLayout* GetInputLayout(UUID shaderID) const`

### `Engine::TextureManager` (`include/Engine/TextureManager.h`)

- `UUID CreateDefaultTexture(ID3D11Device* device, AssetManager& assetManager)` — Generates 1x1 `primitive://white`, `primitive://default_normal`, and `primitive://default_orm` textures.
- `UUID LoadTexture(ID3D11Device* device, AssetManager& assetManager, const std::string& filepath)` — Decodes `RGBA8` via `stb_image` (from VFS or disk) into an SRV; falls back to `m_defaultTextureID` on error.
- `UUID LoadCubemap(ID3D11Device* device, AssetManager& assetManager, const std::vector<std::string>& filepaths)` — Loads 6 faces (`+X, -X, +Y, -Y, +Z, -Z`) with `D3D11_RESOURCE_MISC_TEXTURECUBE`.
- `ID3D11ShaderResourceView* GetTexture(UUID uuid) const` — Returns the cached SRV or `m_defaultSRV`.
- `GetDefaultTexture()`, `GetDefaultNormalMap()`, `GetDefaultORMMap()` — Default fallback UUID getters.

### `Engine::FontManager` (`include/Engine/FontManager.h`)

- **`FontAsset`**: `atlasSRV` (`ComPtr<ID3D11ShaderResourceView>`), `charData[96]` (`stbtt_bakedchar` for ASCII `32..126`), `atlasWidth` (`2048` when baked), `atlasHeight` (`2048` when baked), `fontSize` (`float`).
- `UUID LoadFont(ID3D11Device* device, AssetManager& assetManager, const std::string& filepath, float fontSize)` — Bakes a `.ttf` font (from VFS or disk) into a `2048x2048` white-RGB + alpha-coverage texture under `<filepath>?size=<fontSize>`.
- `FontAsset* GetFont(UUID fontID)`
- `void Shutdown()` — Clears `m_fonts`.

---

## Physics, Audio, Input & Editor Classes

### `Engine::PhysicsManager` & `PhysicsContactListener` (`include/Engine/PhysicsManager.h`)

- **Collision Layers**: `Layers::NON_MOVING` (`0`), `Layers::MOVING` (`1`), implemented by `BPLayerInterfaceImpl`, `ObjectVsBroadPhaseLayerFilterImpl`, and `ObjectLayerPairFilterImpl`.
- **`PhysicsContactListener`**: Inherits `JPH::ContactListener`; overrides `OnContactValidate`, `OnContactAdded`, `OnContactPersisted`, and `OnContactRemoved`, plus `void FlushEvents()` to dispatch Lua collision callbacks on the main thread.
- **`PhysicsManager` Methods**:
    - `bool Initialize(Scene* scene)` / `void Shutdown()`
    - `void Update(float deltaTime)` — Clamps `deltaTime <= 1/30s`, steps Jolt with 4 collision sub-steps, and calls `FlushEvents()`.
    - `JPH::PhysicsSystem* GetSystem() const` / `JPH::BodyInterface& GetBodyInterface()`
    - `JPH::BodyID CreateRigidBody(entt::entity entity, const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager)`
    - `void RemoveRigidBody(JPH::BodyID bodyID)`
    - `entt::entity CastRay(const Math::Ray& ray, entt::registry& registry)`
    - `void ResetBodyTransform(const TransformComponent& tc, RigidBodyComponent& rbc, const MeshManager& meshManager)`

### `Engine::AudioManager` (`include/Engine/AudioManager.h`)

- `bool Initialize()` / `void Shutdown()`
- `void PlaySound2D(UUID assetID, AssetManager& assetManager)` — Fire-and-forget 2D playback (Editor mode only; logs warning in VFS mode).
- `void* LoadSound(UUID assetID, AssetManager& assetManager, bool is3D, bool loop)` — Returns an opaque `AudioInstance*` handle.
- `void PlayAudio(void* soundHandle)` / `void StopAudio(void* soundHandle)` / `void DestroyAudio(void* soundHandle)`
- `void SetAudioPosition(void* soundHandle, float x, float y, float z)` / `void SetAudioVolume(void* soundHandle, float volume)`
- `void SetListenerPosition(float px, float py, float pz, float fx, float fy, float fz)`
- `ma_engine* GetEngine() const`

### `Engine::InputManager` (`include/Engine/InputManager.h`)

Header-only SDL2 keyboard and mouse state tracker.

- **`Engine::Key`** (`uint16_t`): `Unknown`, `A`–`Z`, `Num0`–`Num9`, `Escape`, `LControl`, `LShift`, `LAlt`, `RControl`, `RShift`, `RAlt`, `Space`, `Enter`, `Backspace`, `Tab`, `Up`, `Down`, `Left`, `Right`, `Count`.
- **`Engine::MouseDelta`**: `int dx`, `int dy`, `int wheelY`.
- **Methods**: `void BeginFrame()`, `bool ProcessEvent(const SDL_Event& e)`, `bool IsKeyDown(Key k) const`, `bool IsKeyPressed(Key k) const`, `bool IsKeyReleased(Key k) const`, `MouseDelta GetMouseDelta() const`, `bool IsMouseCaptured() const`, `void SetMouseCaptured(bool enabled)`.

### `Engine::ImGuiManager` & `Engine::EditorUI` (`ImGuiManager.h`, `EditorUI.h`)

Compiled into `DX11Editor` (guarded in `main.cpp` by `#ifndef DIST_BUILD`):

- **`ImGuiManager`**: `bool Initialize(SDL_Window*, ID3D11Device*, ID3D11DeviceContext*)`, `void Shutdown()`, `void BeginFrame()`, `void EndFrame()`, `bool ProcessEvent(const SDL_Event& e)` (returns `io.WantCaptureMouse || io.WantCaptureKeyboard`).
- **`EditorState`**: `enum class EditorState { Edit, Play };`
- **`EditorUI`**:
    - `void Render(Scene&, Renderer&, MeshManager&, TextureManager&, FontManager&, InputManager&, PhysicsManager&, SDL_Window*)`
    - `bool IsSceneFocused() const`
    - `EditorState GetState() const`
    - `entt::entity GetSelectedEntity() const`
