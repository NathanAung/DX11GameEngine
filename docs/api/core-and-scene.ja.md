# コア・Scene・Systems・シリアライズ API

このページでは、`include/Engine/Core.h`、`UUID.h`、`MathUtils.h`、`Scene.h`、`Systems.h`、および `SceneSerializer.h` で宣言されている基盤識別子、数学ユーティリティ、シーンコンテナ、ECS システム、およびシリアライズインターフェースについて説明します。

---

## プリコンパイル済みコアヘッダー (`include/Engine/Core.h`)

`Core.h` は `DX11Editor` と `DX11Runtime` の両方でプリコンパイル済みヘッダー (`target_precompile_headers`) として設定されており、以下をインクルードします。

- C++17 標準ライブラリヘッダー (`<cstdio>`, `<stdexcept>`, `<string>`, `<vector>`, `<cstdint>`)
- SDL2 (`<SDL.h>`, `<SDL_syswm.h>`)
- DirectX 11 (`<d3d11.h>`, `<dxgi.h>`, `<d3dcompiler.h>`, `<wrl/client.h>`, `<DirectXMath.h>`) および `d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`, `dxguid.lib` の `#pragma comment(lib, ...)` ディレクティブ
- Jolt Physics 統合ヘッダー (`"Engine/PhysicsManager.h"`)

---

## `Engine::UUID` (`include/Engine/UUID.h`)

セッション間、JSON シーンファイル、および VFS アーカイブ全体でアセットを一意に識別するための 64 ビット乱数 ID クラスです。`std::unordered_map` のキーとして直接使用できるよう `std::hash<Engine::UUID>` が特殊化されています。

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

`DirectXMath` および `DirectXCollision` 上に構築されたインライン数学ヘルパー群です（左手座標系：`+X` 右、`+Y` 上、`+Z` 前方、NDC 深度 `[0, 1]`）。

### `Engine::Math::Ray`

```cpp
struct Ray
{
    DirectX::XMFLOAT3 origin;
    DirectX::XMFLOAT3 direction;
};
```

### 関数一覧

| シグネチャ | 説明 |
| :--- | :--- |
| `DirectX::XMFLOAT3 QuaternionToEulerDegrees(const DirectX::XMFLOAT4& q)` | `q` を正規化して回転行列を構築し、`[-1, 1]` の `asinf` クランプと ±90° ジンバルロック対策を施した上で度数法の `(pitch, yaw, roll)` を抽出します。 |
| `DirectX::XMFLOAT4 EulerDegreesToQuaternion(const DirectX::XMFLOAT3& euler)` | 度数法の `(pitch, yaw, roll)` をラジアンに変換し、`XMQuaternionRotationRollPitchYaw` で正規化済みクォータニオンを返します。 |
| `Ray ScreenToWorldRay(float mouseX, float mouseY, float screenW, float screenH, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj)` | `(mouseX, mouseY)` を NDC `z = 0.0f` および `z = 1.0f` で逆投影 (`XMVector3Unproject`) し、正規化されたワールド空間のピッキングレイを生成します。 |
| `bool RayIntersectsOBB(const Ray& ray, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& rotation, float& outDistance)` | 中心 `position`、半径（ハーフエクステント）`0.5f * scale`、回転 `rotation` を持つ単位 `DirectX::BoundingOrientedBox` との交差判定を行い、ヒット距離を `outDistance` に書き込みます。 |
| `std::vector<DirectX::XMVECTOR> GetFrustumCornersWorldSpace(const DirectX::XMMATRIX& viewProj)` | DirectX NDC キューブの8頂点（`x, y in {-1, 1}`, `z in {0, 1}`）を透視除算付きで `Inverse(viewProj)` 変換します。CSM のカスケードフィッティングに使用されます。 |

---

## `Engine::Scene` (`include/Engine/Scene.h`)

EnTT エンティティレジストリ、Play モード用バックアップレジストリ、組み込み `sol::state` Lua VM、エンティティ生成ファクトリ、侵入型階層リンク、および遅延破棄キューを統括する中心コンテナです。

### パブリックフィールド

| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `registry` | `entt::registry` | — | メインのライブ ECS レジストリ。`view<>` クエリやコンポーネント操作を直接行えるよう意図的に公開されています。 |
| `m_backupRegistry` | `entt::registry` | — | `Edit <-> Play` 遷移および `Game.Restart()` 時にプレイ前の状態を保持するスナップショットレジストリ。 |
| `m_activeRenderCamera` | `entt::entity` | `entt::null` | 現在レンダリングおよび 3D オーディオリスナーの姿勢を制御しているカメラのエンティティハンドル。 |
| `m_lua` | `sol::state` | — | サンドボックス化された各 `ScriptInstance` 環境が共有するマスター Lua 仮想マシン状態。 |
| `m_requestQuit` | `bool` | `false` | `Game.Quit()` により設定され、現在フレームの終了時に `main()` へ終了を指示します。 |
| `m_requestRestart` | `bool` | `false` | `Game.Restart()` により設定され、描画後に `m_backupRegistry` の復元とスクリプト再初期化を `main()` へ指示します。 |
| `m_assetManager` | `Engine::AssetManager*` | `nullptr` | 実行時インポートや VFS 読み込みに使用するエンジン `AssetManager` へのポインタ。 |
| `m_currentScenePath` | `std::string` | `""` | 現在ロードまたは保存されている JSON シーンのファイルパス（未保存時は空文字列）。 |
| `m_entitiesToDestroy` | `std::vector<std::pair<entt::entity, float>>` | `{}` | `(entity, 残り秒数)` のペアを保持する遅延破棄キュー。 |

### 依存マネージャーおよびパスのアクセサ

| メソッド | 説明 |
| :--- | :--- |
| `Engine::PhysicsManager* GetPhysicsManager() const` | `InitializeLuaBindings` 時に設定された `PhysicsManager*` を返します。 |
| `void SetAudioManager(Engine::AudioManager* am)` / `GetAudioManager() const` | サウンドハンドルのクリーンアップ用に `AudioManager*` を設定・取得します。 |
| `void SetAssetManager(Engine::AssetManager* am)` / `GetAssetManager() const` | アセット検索用に `AssetManager*` を設定・取得します。 |
| `void SetCurrentScenePath(const std::string& path)` / `GetCurrentScenePath() const` | アクティブな `.json` シーンファイルパスを設定・取得します。 |
| `void SetDefaultAssets(UUID shaderID, UUID debugShaderID, UUID skyboxShaderID, UUID spriteShaderID, UUID cubeID, UUID sphereID, UUID capsuleID, UUID quadMeshID)` | コアシェーダーおよびプリミティブメッシュの UUID をシーンにキャッシュします（`GetDefaultShaderID()`, `GetDebugShaderID()`, `GetSkyboxShaderID()`, `GetSpriteShaderID()`, `GetCubeMeshID()`, `GetSphereMeshID()`, `GetCapsuleMeshID()`, `GetQuadMeshID()` で取得）。 |

### エンティティ生成・検索メソッド

| シグネチャ | 説明 |
| :--- | :--- |
| `entt::entity CreateEntity(const std::string& name)` | 自動インクリメントされる `IDComponent`、`NameComponent{name}`、および単位変換の `TransformComponent` を持つ汎用エンティティを生成します。 |
| `entt::entity CreateSampleEntity(const std::string& name)` | 座標 `(0, 0, 5)`、スケール `0.1` に配置された `MeshRendererComponent` 付きエンティティを生成します。 |
| `entt::entity CreateCube(const std::string& name)` | `m_cubeMeshID` が割り当てられた `MeshRendererComponent` 付きエンティティを生成します。 |
| `entt::entity CreateSphere(const std::string& name)` | `m_sphereMeshID` が割り当てられた `MeshRendererComponent` 付きエンティティを生成します。 |
| `entt::entity CreateCapsule(const std::string& name)` | `m_capsuleMeshID` が割り当てられた `MeshRendererComponent` 付きエンティティを生成します。 |
| `entt::entity CreateEditorCamera(const std::string& name, unsigned width, unsigned height)` | 座標 `(0, 0, -10)` に `CameraComponent`、`ViewportComponent{width, height}`、`EditorCamControlComponent` を持つカメラを生成します。`m_activeRenderCamera` が `entt::null` の場合は自動的に設定します。 |
| `entt::entity CreateGameCamera(const std::string& name, unsigned width, unsigned height)` | 座標 `(0, 0, -10)` に `CameraComponent` と `ViewportComponent{width, height}` を持つゲーム内カメラを生成します（`EditorCamControlComponent` は付与されません）。 |
| `entt::entity CreateDirectionalLight(const char* name)` | `+X` 軸周りに 45° 下向きに傾けた白色の `LightType::Directional` ライト (`intensity = 5.0f`) を生成します。 |
| `entt::entity CreatePointLight(const char* name, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& color, float intensity, float range)` | 指定した `position`、`color`、`intensity`、`range` を持つ `LightType::Point` エンティティを生成します。 |
| `entt::entity CreateSpotLight(const char* name, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& color, float intensity, float range, float spotAngleRadians)` | `+Z` 軸を `direction` に向けるクォータニオンを構築し、`position` に `LightType::Spot` エンティティを生成します。 |
| `entt::entity InstantiateModel(const std::string& filepath, Engine::Renderer& renderer, Engine::MeshManager& meshManager)` | `MeshManager::LoadModel` で `filepath` を読み込み、ファイル名（拡張子なし）の親エンティティと、サブメッシュごとの子エンティティ (`<stem>_mesh_<i>`) を生成して階層化します。 |
| `entt::entity FindEntityByName(const std::string& name)` | `NameComponent` を線形探索し、最初に見つかった `entt::entity`（見つからない場合は `entt::null`）を返します。 |
| `entt::entity CastRay(const Engine::Math::Ray& ray, Engine::PhysicsManager& physicsManager)` | まず Jolt のナローフェーズ・レイキャストを実行し、ヒットしなかった場合はリジッドボディを持たない有効なエンティティ（`m_activeRenderCamera` を除く）に対して `Math::RayIntersectsOBB` で判定します。 |

### 階層・ライフサイクル・Play ステート制御メソッド

| シグネチャ | 説明 |
| :--- | :--- |
| `void ParentEntity(entt::entity child, entt::entity parent)` | 祖先の循環参照チェックを行い、既存の親から切り離した上で、ワールド変換を維持するよう `parent` 基準のローカル変換へ再計算し、`parent` の侵入型子リストへ追加します。 |
| `void UnparentEntity(entt::entity child)` | 親の兄弟リストから `child` を取り外し、ワールド変換を維持するようキャッシュ済み `worldMatrix` をローカル TRS へ分解します。 |
| `void DestroyEntity(entt::entity entity, Engine::PhysicsManager& physicsManager)` | 子エンティティを再帰的に破棄し、親子解除、Jolt リジッドボディの削除、Lua `OnDestroy()` コールバックの実行を行った後、EnTT ハンドルを破棄します。 |
| `void SubmitForDestruction(entt::entity entity, float delay = 0.0f)` | フレーム終端で安全に削除するため、`m_entitiesToDestroy` に `(entity, delay)` を登録します（重複は無視されます）。 |
| `void ProcessDestructionQueue(Engine::PhysicsManager& physicsManager, float deltaTime)` | キュー内のタイマーを減算し、残り時間が `<= 0.0f` になったエントリに対して `DestroyEntity` を呼び出します。 |
| `void CopyToBackup()` | すべてのエンティティ（同一の `entt::entity` ID を維持）およびコンポーネントを `registry` から `m_backupRegistry` へ複製します。 |
| `void RestoreFromBackup(Engine::PhysicsManager& physicsManager)` | アクティブな Jolt ボディとオーディオハンドルをすべて破棄して `registry` をクリアし、実行時ハンドルをリセットした状態で `m_backupRegistry` から全エンティティ・コンポーネントを復元します。 |
| `void StartPlayMode()` | `CopyToBackup()` と `ScriptSystemInit(*this)` を実行し、`m_activeRenderCamera` を最初の有効な非エディタカメラへ切り替えます。 |
| `void StopPlayMode(entt::entity editorCameraFallback, Engine::PhysicsManager& physicsManager)` | Lua スクリプトをシャットダウンして `RestoreFromBackup(physicsManager)` を呼び出し、`m_activeRenderCamera = editorCameraFallback` に戻します。 |
| `void InitializeLuaBindings(Engine::InputManager* inputManager, Engine::PhysicsManager* physicsManager)` | Lua 標準ライブラリを開き、エンジンの全ユーザー型、列挙型、グローバルテーブルを `m_lua` に登録します。 |
| `void Clear(Engine::PhysicsManager& physicsManager)` | すべての Jolt ボディとオーディオハンドルを破棄し、`registry` と `m_entitiesToDestroy` をクリアして、`m_activeRenderCamera` と `m_currentScenePath` をリセットします。 |
| `void GenerateDefaultSetup(Engine::PhysicsManager& physicsManager)` | シーンをクリアし、`Editor Camera`、`Main Camera`、`Sun Light`、および `y = -5` の位置に静的な `20 x 0.1 x 20` の `Ground` ボックスを生成します。 |

---

## `Engine::SceneSerializer` (`include/Engine/SceneSerializer.h`)

RapidJSON を用いた静的な JSON シーン保存および4パス読み込みクラスです。

| 静的メソッド | 説明 |
| :--- | :--- |
| `static bool Serialize(const std::string& filepath, Scene& scene)` | `IDComponent` を持つ全エンティティをトップレベルの `"Scene"` JSON 配列として `filepath` に書き出し、アタッチされた Lua スクリプトとシーン自身を `AssetManager` に登録して `enginefiles/AssetRegistry.json` を保存します。 |
| `static bool Deserialize(const std::string& filepath, Scene& scene, PhysicsManager& physicsManager, AssetManager& assetManager, std::string& outErrorMsg)` | VFS（有効時）またはディスクから `filepath` を読み込み、現在のワールドを変更する **前に** 参照されている全アセット UUID とスクリプトファイルを検証します。検証通過後にシーンをクリアし、エンティティ・コンポーネントの再構築と `RelationshipComponent` 階層リンクの復元を行います。失敗時は `outErrorMsg` に理由を格納します。 |

---

## ECS システム (`include/Engine/Systems.h`)

`Engine::Scene` に対して動作するステートレスな更新および描画パス関数群です。

### シミュレーションおよびトランスフォームシステム

| 関数シグネチャ | 説明 |
| :--- | :--- |
| `void PhysicsSystem(Scene& scene, PhysicsManager& physicsManager, const MeshManager& meshManager, float dt, bool isPlaying)` | 有効フラグに応じて Jolt ボディを生成・削除します。`isPlaying` が `true` の場合は Jolt をステップ実行 (`dt`) してワールド変換をローカル `TransformComponent` に書き戻し、`false` の場合は ECS のワールド行列を Jolt に反映して運動量をリセットします。 |
| `void ScriptSystemInit(Scene& scene)` | 分離された `sol::environment` インスタンス（`self` バインド済み）を生成し、VFS またはディスクから `.lua` ファイルを読み込んでコールバックをキャッシュし、`OnCreate()` を呼び出します。 |
| `void ScriptSystemUpdate(Scene& scene, float dt)` | `Time.deltaTime = dt` を更新し、実行時に新規追加されたスクリプトの遅延初期化 (`OnCreate`) を行った上で、`sol::protected_function` 経由で `OnUpdate(dt)` を呼び出します。 |
| `void ScriptSystemShutdown(Scene& scene)` | 初期化済みの全スクリプトに対して `OnDestroy()` を呼び出し、スクリプト環境およびコールバックハンドルをリセットします。 |
| `void AudioSystem(Scene& scene, AudioManager& audioManager, AssetManager& assetManager)` | 3D リスナーを `m_activeRenderCamera` に同期し、`AudioComponent` ハンドルの遅延ロード、`playOnCreate` クリップの再生開始、非アクティブエンティティの停止、3D エミッター座標と音量の更新を行います。 |
| `void EditorCameraInputSystem(Scene& scene, const InputManager& input, float dt, bool isSceneFocused)` | 右クリックキャプチャ時の `EditorCamControlComponent` のヨー・ピッチ更新（±89° クランプ）、フォーカス時のホイールズーム、および `WASD` + `LShift` 移動を処理します。 |
| `void TransformSystem(Scene& scene)` | ルートエンティティを走査し、`RelationshipComponent` の兄弟チェーンに沿って `World = Local * ParentWorld` を再帰的に伝播します。 |
| `void CameraMatrixSystem(Scene& scene, Renderer& renderer)` | `m_activeRenderCamera` から左手系のビュー行列 (`Inverse(world)`) と透視投影行列 (`XMMatrixPerspectiveFovLH`) を計算し、`Renderer` にアップロードします。 |
| `void DemoRotationSystem(Scene& scene, entt::entity sampleEntity, float dt)` | `sampleEntity` を X 軸・Y 軸周りに連続回転させるデモ用ユーティリティシステムです。 |

### `Engine::RenderSystem` 関数群

| 関数シグネチャ | 説明 |
| :--- | :--- |
| `void RenderSystem::DrawEntities(Scene& scene, MeshManager& meshManager, ShaderManager& shaderManager, Renderer& renderer, TextureManager& textureManager, FontManager& fontManager)` | 3分割カスケードシャドウ深度パス (`ShadowVS`) の実行、メインレンダーターゲットの復元、最大16ライトの視錐台カリングと距離ソート、未ロードメッシュ・テクスチャの JIT 読み込み、および `UnlitColor` / `LitColor` / `Textured` PBR エンティティの描画を行います。 |
| `void RenderSystem::DrawDebugColliders(Scene& scene, Renderer& renderer, MeshManager& meshManager, ShaderManager& shaderManager, entt::entity selectedEntity)` | `selectedEntity` の `RigidBodyComponent` コライダー形状とスケールに一致する鮮やかな緑色の Unlit ワイヤーフレームメッシュを描画します。 |
| `void RenderSystem::DrawUI(Scene& scene, Renderer& renderer, MeshManager& meshManager, ShaderManager& shaderManager, TextureManager& textureManager, FontManager& fontManager)` | ピクセル平行投影とアルファブレンディングを設定し、有効なすべての `SpriteRendererComponent` および `TextRendererComponent` エンティティを描画します（必要に応じて `.ttf` フォントアトラスを JIT ベイクします）。 |
