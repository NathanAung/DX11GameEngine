# Renderer・各種マネージャー・Editor UI API

このページでは、`include/Engine/` 配下で宣言されている DirectX 11 レンダラー、各種リソース・アセットマネージャー、物理・オーディオ・入力サブシステム、および Dear ImGui エディタクラスについて説明します。

---

## `Engine::Renderer` (`include/Engine/Renderer.h`)

Direct3D 11 デバイス、イミディエイトコンテキスト、スワップチェーン、定数バッファ、パイプラインステートオブジェクト、オフスクリーンのエディタ用フレームバッファ、およびカスケードシャドウマップ用リソースをカプセル化します。

### GPU データ構造体

- **`DX11Context`**: `device`、`context`、`swapChain`、`rtv`、`depthStencilBuffer`、`dsv` の各 `ComPtr` ハンドルと、`featureLevel`（デフォルト `D3D_FEATURE_LEVEL_11_0`）、`width` (`1280`)、`height` (`720`) を保持します。
- **`LightData`** *(64 バイト、16 バイトアライン)*: `position` (`XMFLOAT3`)、`range` (`float`)、`direction` (`XMFLOAT3`)、`spotAngle` (`float`)、`color` (`XMFLOAT3`)、`intensity` (`float`)、`type` (`unsigned int`: `0=Dir, 1=Point, 2=Spot`)、`padding` (`XMFLOAT3`)。
- **`LightConstants`** *(PS `b3`, `#define MAX_LIGHTS 16`)*: `cameraPos` (`XMFLOAT3`)、`lightCount` (`unsigned int`)、`lights[MAX_LIGHTS]` (`LightData`)。
- **`MaterialConstants`** *(PS `b4`, 32 バイト)*: `baseColor` (`XMFLOAT4`)、`roughness` (`float`)、`metallic` (`float`)、`padding[2]` (`float`)。
- **`ShadowConstants`** *(PS `b5`)*: `cascadeViewProj[3]` (`XMFLOAT4X4`)、`cascadeSplits` (`XMFLOAT4`: `x=near, y=mid, z=far`)。
- **`Renderer::CascadeData`** (`static const int NUM_CASCADES = 3`): `viewProj` (`XMMATRIX`)、`splitDepth` (`float`)。

### パブリックメソッド

| カテゴリ | シグネチャ | 説明 |
| :--- | :--- | :--- |
| **ライフサイクル** | `bool InitD3D11(HWND hwnd, unsigned width, unsigned height)` | デバイス、スワップチェーン、RTV/DSV、パイプラインステート、定数バッファ、CSM テクスチャ、およびエディタ用フレームバッファ (`#ifndef DIST_BUILD`) を初期化します。 |
| | `void Shutdown()` | すべての `ComPtr` GPU ステート、バッファ、ビュー、スワップチェーン、コンテキスト、デバイスを解放します。 |
| | `void Present(bool vsync)` | スワップチェーンを画面に表示します (`SyncInterval = vsync ? 1 : 0`)。 |
| | `bool Resize(unsigned width, unsigned height)` | スワップチェーンバッファ、メイン RTV/DSV、およびエディタ用フレームバッファをリサイズします（最小化時の `0x0` は無視）。 |
| **レンダーターゲット** | `void BeginFrame()` | メイン RTV/DSV をバインドしてクリアし、ビューポート・各種ステートを設定して VS `b0`–`b2` をバインドします。 |
| | `bool CreateFramebuffer(UINT width, UINT height)` | オフスクリーンの `R8G8B8A8_UNORM` テクスチャ (`RTV` + `SRV`) と `D24_UNORM_S8_UINT` 深度ターゲットを生成・リサイズします。 |
| | `void BindFramebuffer()` | オフスクリーンのエディタ用フレームバッファをバインドしてクリアし、ラスタライザ・深度・VS 定数バッファのステートを再設定します。 |
| | `void BindBackBuffer()` | メインスワップチェーンのバックバッファをバインドし、黒色でクリアします。 |
| | `ID3D11ShaderResourceView* GetFramebufferSRV() const` | `ImGui::Image` 表示用のオフスクリーンシーン SRV を返します。 |
| **定数バッファと描画** | `void UpdateProjectionMatrix(const DirectX::XMMATRIX& proj)` | VS `b0` を更新します。 |
| | `void UpdateViewMatrix(const DirectX::XMMATRIX& view)` | VS `b1` を更新します。 |
| | `void UpdateWorldMatrix(const DirectX::XMMATRIX& world)` | VS `b2` を更新します。 |
| | `void UpdateLightConstants(const LightConstants& data)` | PS `b3` にデータをアップロードしてバインドします。 |
| | `void UpdateMaterialConstants(const MaterialConstants& material)` | PS `b4` にデータをアップロードしてバインドします。 |
| | `void UpdateColorConstants(const DirectX::XMFLOAT4& color)` | PS `b0` (`UnlitPS` / `SpritePS`) を動的にマップ (`WRITE_DISCARD`) して更新・バインドします。 |
| | `void UpdateShadowConstants(const ShadowConstants& data)` | PS `b5` にデータをアップロードしてバインドします。 |
| | `void BindShader(const ShaderManager& shaderMan, UUID shaderID)` | `ShaderManager` から VS、PS、および入力レイアウトをバインドします。 |
| | `void SubmitMesh(const MeshBuffers& mesh, ID3D11InputLayout* inputLayout)` | 入力レイアウト、頂点バッファ、インデックスバッファ、および `TRIANGLELIST` トポロジをバインドします。 |
| | `void DrawIndexed(UINT indexCount)` | `ID3D11DeviceContext::DrawIndexed(indexCount, 0, 0)` を発行します。 |
| **シャドウ (CSM)** | `void CalculateCascadeMatrices(const DirectX::XMMATRIX& camView, float camFov, float aspect, float camNear, float camFar, const DirectX::XMFLOAT3& lightDir)` | 全3カスケード（`5%`, `20%`, `100%`）のテクセルスナップ済みライト空間 `viewProj` 行列と分割深度を算出します。 |
| | `const std::vector<CascadeData>& GetCascades() const` | 計算済みのカスケード行列と分割深度の配列を返します。 |
| | `void BindShadowPass(int cascadeIndex)` | PS SRV (`0..15`) をアンバインドし、`m_shadowDSVs[cascadeIndex]` のバインド、深度クリア、`2048x2048` ビューポート設定、`m_shadowRasterState` 設定、および `PS = nullptr` 設定を行います。 |
| | `ID3D11ShaderResourceView* GetShadowSRV() const` | 3スライスの `R32_FLOAT` `Texture2DArray` SRV (`t3`) を返します。 |
| **スカイボックス・各種ステート** | `void SetSkybox(ID3D11ShaderResourceView* srv)` | アクティブなキューブマップ SRV を設定します。 |
| | `void DrawSkybox(const MeshManager& meshMan, const ShaderManager& shaderMan, const CameraComponent& camComp, const TransformComponent& camTrans, UUID cubeMeshID, UUID skyboxShaderID)` | 平行移動成分を除去したビュー行列と `LESS_EQUAL` 深度ステートを用いてキューブマップ・スカイボックスを描画します。 |
| | `void SetWireframeMode(bool enable)` | `m_wireframeRasterizerState` (`CULL_NONE`) と `m_solidRasterizerState` を切り替えます。 |
| | `void SetUIBlendState(bool enable)` / `SetUIDepthState(bool enable)` | 2D UI 描画用のアルファブレンドステートおよび深度無効ステートを切り替えます。 |
| **アクセサ** | `GetDevice()`, `GetContext()`, `GetRTV()`, `GetDSV()`, `GetRasterState()`, `GetDepthStencilState()`, `GetSamplerState()`, `GetLightCB()`, `GetWidth()`, `GetHeight()` | 低レベル D3D11 オブジェクトおよび画面サイズのゲッター群。 |

---

## `Engine::AssetManager` (`include/Engine/AssetManager.h`)

アセット UUID の追跡、`enginefiles/AssetRegistry.json` の永続化、`PAK2` VFS アーカイブのパック・マウント、およびスタンドアロンビルドのエクスポートを担当します。

### 型定義

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

### パブリックメソッド

| シグネチャ | 説明 |
| :--- | :--- |
| `UUID ImportAsset(const std::string& filepath, AssetType type)` | ファイルの存在確認（Editor ではディスク上、VFS 有効時はレジストリ内を確認。仮想 URI の `primitive://`、`shader://`、`cubemap://` はスキップ）を行い、`filepath` で重複排除した上で `UUID` を返します（失敗時は `0`）。 |
| `const AssetMetadata* GetMetadata(UUID handle) const` | `handle` に対応するメタデータを検索します（未登録時は `nullptr`）。 |
| `void SetAssetLoaded(UUID handle, bool loaded)` | `AssetMetadata::isLoaded` を更新します。 |
| `const std::unordered_map<UUID, AssetMetadata>& GetRegistry() const` | アセットマップ全体への読み取り専用参照を返します。 |
| `bool SaveRegistry(const std::string& filepath)` | 認識されている全アセットを JSON に書き出します（VFS 有効時は何もせず `true` を返します）。 |
| `bool LoadRegistry(const std::string& filepath)` | JSON から UUID マッピングを `isLoaded = false` として復元します。 |
| `bool PackAssets(const std::string& pakFilepath) const` | 登録済み全エントリと物理ファイルのバイナリペイロードを `PAK2` アーカイブへ書き出します。 |
| `bool MountVFS(const std::string& pakFilepath)` | `"PAK2"` ヘッダーを検証し、`m_assetRegistry` と `m_vfsTable` を構築して VFS モード (`IsVFSActive() == true`) を有効化します。 |
| `std::vector<char> ReadAssetFromVFS(UUID handle) const` | `m_vfsPakPath` 内の `TOCEntry::offset` にシークし、`size` バイト分の生データを返します。 |
| `bool IsInVFS(UUID handle) const` / `bool IsVFSActive() const` | VFS アーカイブ内の存在確認および VFS マウント状態を返します。 |
| `bool ExportProject(Scene& activeScene, std::string& outErrorMsg) const` | `activeScene` の保存、`ExportedGame/data.pak` の生成、`RuntimeTemplate/` のコピー、`ExportedGame/enginefiles/Launch.txt` の書き出し、およびエクスプローラー起動を行います。 |

---

## リソースマネージャー群

### `Engine::MeshManager` (`include/Engine/MeshManager.h`)

- **構造体**:
    - `Vertex`: `position` (`XMFLOAT3`)、`normal` (`XMFLOAT3`)、`texCoord` (`XMFLOAT2`)、`tangent` (`XMFLOAT3`)。
    - `MeshBuffers`: `vertexBuffer` (`ID3D11Buffer*`)、`indexBuffer` (`ID3D11Buffer*`)、`indexCount` (`UINT`)、`stride` (`UINT`)、`indexFormat` (`DXGI_FORMAT_R32_UINT`)。
- **メソッド**:
    - `UUID InitializeCube(ID3D11Device* device, AssetManager& assetManager)` — 単位キューブ (`primitive://cube`) を生成します。
    - `UUID InitializeQuad(ID3D11Device* device, AssetManager& assetManager)` — 2D スプライト・テキスト用の `[0..1]` XY 単位クアッド (`primitive://quad`) を生成します。
    - `UUID CreateSphere(ID3D11Device* device, AssetManager& assetManager, float radius, int slices, int stacks)`
    - `UUID CreateCapsule(ID3D11Device* device, AssetManager& assetManager, float radius, float cylinderHeight, int slices, int stacks)`
    - `std::vector<UUID> LoadModel(ID3D11Device* device, AssetManager& assetManager, const std::string& filename)` — Assimp（`VFSIOSystem` 有効）を用いてモデルをインポートし、`filename` でキャッシュした上でサブメッシュ UUID の配列 (`<filename>?mesh=<i>`) を返します。
    - `bool GetMesh(UUID meshID, MeshBuffers& out) const`
    - `const std::vector<DirectX::XMFLOAT3>& GetMeshPositions(UUID meshID) const` / `GetMeshIndices(UUID meshID) const` — Jolt の `MeshShape` / `ConvexHullShape` 生成用 CPU キャッシュ頂点・インデックス配列を取得します。

### `Engine::ShaderManager` (`include/Engine/ShaderManager.h`)

- `UUID LoadBasicShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://basic` -> `BasicVS` + `BasicPS`)
- `UUID LoadSkyboxShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://skybox` -> `SkyboxVS` + `SkyboxPS`)
- `UUID LoadUnlitShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://unlit` -> `UnlitVS` + `UnlitPS`)
- `UUID LoadSpriteShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://sprite` -> `SpriteVS` + `SpritePS`)
- `UUID LoadShadowShaders(ID3D11Device* device, AssetManager& assetManager)` (`shader://shadow` -> `ShadowVS`, `ps = nullptr`)
- `void Bind(UUID shaderID, ID3D11DeviceContext* context) const`
- `ID3D11InputLayout* GetInputLayout(UUID shaderID) const`

### `Engine::TextureManager` (`include/Engine/TextureManager.h`)

- `UUID CreateDefaultTexture(ID3D11Device* device, AssetManager& assetManager)` — 1x1 の `primitive://white`、`primitive://default_normal`、`primitive://default_orm` テクスチャを生成します。
- `UUID LoadTexture(ID3D11Device* device, AssetManager& assetManager, const std::string& filepath)` — `stb_image` を用いて VFS またはディスクから `RGBA8` 画像をデコードし SRV を生成します（失敗時は `m_defaultTextureID` にフォールバック）。
- `UUID LoadCubemap(ID3D11Device* device, AssetManager& assetManager, const std::vector<std::string>& filepaths)` — `D3D11_RESOURCE_MISC_TEXTURECUBE` 付きで6面画像 (`+X, -X, +Y, -Y, +Z, -Z`) をロードします。
- `ID3D11ShaderResourceView* GetTexture(UUID uuid) const` — キャッシュされた SRV または `m_defaultSRV` を返します。
- `GetDefaultTexture()`, `GetDefaultNormalMap()`, `GetDefaultORMMap()` — デフォルトフォールバックテクスチャの UUID ゲッター。

### `Engine::FontManager` (`include/Engine/FontManager.h`)

- **`FontAsset`**: `atlasSRV` (`ComPtr<ID3D11ShaderResourceView>`)、`charData[96]`（ASCII `32..126` 用の `stbtt_bakedchar`）、`atlasWidth`（ベイク時 `2048`）、`atlasHeight`（ベイク時 `2048`）、`fontSize` (`float`)。
- `UUID LoadFont(ID3D11Device* device, AssetManager& assetManager, const std::string& filepath, float fontSize)` — VFS またはディスクから `.ttf` フォントを読み込み、白 RGB + アルファカバレッジ形式の `2048x2048` テクスチャへベイクして `<filepath>?size=<fontSize>` として登録します。
- `FontAsset* GetFont(UUID fontID)`
- `void Shutdown()` — `m_fonts` をクリアします。

---

## 物理・オーディオ・入力・エディタ系クラス

### `Engine::PhysicsManager` & `PhysicsContactListener` (`include/Engine/PhysicsManager.h`)

- **コリジョンレイヤー**: `Layers::NON_MOVING` (`0`)、`Layers::MOVING` (`1`)。`BPLayerInterfaceImpl`、`ObjectVsBroadPhaseLayerFilterImpl`、`ObjectLayerPairFilterImpl` によって実装されています。
- **`PhysicsContactListener`**: `JPH::ContactListener` を継承し、`OnContactValidate`、`OnContactAdded`、`OnContactPersisted`、`OnContactRemoved` をオーバーライドします。さらにメインスレッド上で Lua 衝突コールバックをディスパッチする `void FlushEvents()` を備えています。
- **`PhysicsManager` メソッド**:
    - `bool Initialize(Scene* scene)` / `void Shutdown()`
    - `void Update(float deltaTime)` — `deltaTime <= 1/30s` にクランプし、4回のコリジョンサブステップで Jolt を更新した後、`FlushEvents()` を呼び出します。
    - `JPH::PhysicsSystem* GetSystem() const` / `JPH::BodyInterface& GetBodyInterface()`
    - `JPH::BodyID CreateRigidBody(entt::entity entity, const TransformComponent& tc, const RigidBodyComponent& rbc, const MeshManager& meshManager)`
    - `void RemoveRigidBody(JPH::BodyID bodyID)`
    - `entt::entity CastRay(const Math::Ray& ray, entt::registry& registry)`
    - `void ResetBodyTransform(const TransformComponent& tc, RigidBodyComponent& rbc, const MeshManager& meshManager)`

### `Engine::AudioManager` (`include/Engine/AudioManager.h`)

- `bool Initialize()` / `void Shutdown()`
- `void PlaySound2D(UUID assetID, AssetManager& assetManager)` — ファイア・アンド・フォーゲット方式の簡易 2D 再生（Editor モード専用。VFS モードでは警告を出力します）。
- `void* LoadSound(UUID assetID, AssetManager& assetManager, bool is3D, bool loop)` — 不透明な `AudioInstance*` ハンドルを返します。
- `void PlayAudio(void* soundHandle)` / `void StopAudio(void* soundHandle)` / `void DestroyAudio(void* soundHandle)`
- `void SetAudioPosition(void* soundHandle, float x, float y, float z)` / `void SetAudioVolume(void* soundHandle, float volume)`
- `void SetListenerPosition(float px, float py, float pz, float fx, float fy, float fz)`
- `ma_engine* GetEngine() const`

### `Engine::InputManager` (`include/Engine/InputManager.h`)

ヘッダーオンリーの SDL2 キーボード・マウス状態トラッカーです。

- **`Engine::Key`** (`uint16_t`): `Unknown`, `A`–`Z`, `Num0`–`Num9`, `Escape`, `LControl`, `LShift`, `LAlt`, `RControl`, `RShift`, `RAlt`, `Space`, `Enter`, `Backspace`, `Tab`, `Up`, `Down`, `Left`, `Right`, `Count`。
- **`Engine::MouseDelta`**: `int dx`, `int dy`, `int wheelY`。
- **メソッド**: `void BeginFrame()`, `bool ProcessEvent(const SDL_Event& e)`, `bool IsKeyDown(Key k) const`, `bool IsKeyPressed(Key k) const`, `bool IsKeyReleased(Key k) const`, `MouseDelta GetMouseDelta() const`, `bool IsMouseCaptured() const`, `void SetMouseCaptured(bool enabled)`。

### `Engine::ImGuiManager` & `Engine::EditorUI` (`ImGuiManager.h`, `EditorUI.h`)

`DX11Editor` に組み込まれるエディタ UI クラス群です（`main.cpp` 内で `#ifndef DIST_BUILD` によりガードされています）。

- **`ImGuiManager`**: `bool Initialize(SDL_Window*, ID3D11Device*, ID3D11DeviceContext*)`、`void Shutdown()`、`void BeginFrame()`、`void EndFrame()`、`bool ProcessEvent(const SDL_Event& e)`（`io.WantCaptureMouse || io.WantCaptureKeyboard` を返します）。
- **`EditorState`**: `enum class EditorState { Edit, Play };`
- **`EditorUI`**:
    - `void Render(Scene&, Renderer&, MeshManager&, TextureManager&, FontManager&, InputManager&, PhysicsManager&, SDL_Window*)`
    - `bool IsSceneFocused() const`
    - `EditorState GetState() const`
    - `entt::entity GetSelectedEntity() const`
