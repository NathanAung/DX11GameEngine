# Lua スクリプティングバインディング

ゲームプレイスクリプトは **Lua** と **sol2** によって動作します。以下に記載されているすべてのバインディングは、`Scene::InitializeLuaBindings` (`src/Engine/Scene.cpp`) および `Engine::ScriptEntity` (`include/Engine/ScriptEntity.h`) の実装と厳密に照合されています。

---

## 実行環境と標準ライブラリ

- **ロードされる標準ライブラリ**: `base`、`math`、`table`、`string` (`m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string)`)。
- **スクリプト単位のサンドボックス化**: 各 `ScriptInstance` は `m_lua.globals()` を継承した独自の `sol::environment` 内で実行され、所有エンティティを指す `Entity` (`Engine::ScriptEntity`) 型の変数 `self` があらかじめ注入されています。

---

## ライフサイクルおよび物理コールバック

`.lua` スクリプト内で以下のグローバル関数を任意の組み合わせで定義すると、`ScriptSystem` および `PhysicsContactListener` が自動的にキャッシュして呼び出します。

| コールバックシグネチャ | 呼び出しタイミング |
| :--- | :--- |
| `function OnCreate()` | Play モード開始時に1回、または `AddLuaScript` で動的にアタッチされた場合は初回の `OnUpdate` 直前に1回呼び出されます。 |
| `function OnUpdate(dt)` | Play モード中、毎フレーム呼び出されます。`dt` (`number`) は秒単位のフレーム経過時間です。 |
| `function OnDestroy()` | エンティティ破棄時 (`Scene::DestroyEntity`)、または Play モードの停止・再起動時に呼び出されます。 |
| `function OnCollisionEnter(other)` | 新しい Jolt 接触点が追加された際、`PhysicsManager::Update` 完了後にメインスレッド上で呼び出されます。`other` は衝突相手の `Entity` (`ScriptEntity`) です。 |
| `function OnCollisionExit(other)` | Jolt の接触ペアが離れた際、メインスレッド上で呼び出されます。`other` は衝突相手の `Entity` (`ScriptEntity`) です。 |

---

## グローバルテーブルとシングルトン

> **呼び出し構文の規約:** `Input` はバインドされた C++ `InputManager*` ユーザー型インスタンスであるため、メソッド呼び出しには **コロン (`:`)** 構文（例: `Input:IsKeyPressed(Key.Space)`）を使用します。一方、`Physics`、`Scene`、`Time`、`Game` はクロージャやフィールドを格納した通常の Lua テーブルであるため、**ドット (`.`)** 構文（例: `Physics.CastRay(...)`、`Scene.FindEntityByName(...)`、`Game.Restart()`）でアクセスします。

### `Input` (`Engine::InputManager` ユーザー型)

| Lua 呼び出し | 戻り値型 | 説明 |
| :--- | :--- | :--- |
| `Input:IsKeyDown(key)` | `boolean` | `key`（`Key` 列挙型）が押下されている間、毎フレーム `true` を返します。 |
| `Input:IsKeyPressed(key)` | `boolean` | `key` が離された状態から押された **最初のフレームのみ** `true` を返します。 |
| `Input:IsKeyReleased(key)` | `boolean` | `key` が押された状態から離された **最初のフレームのみ** `true` を返します。 |

### `Physics` テーブル

| Lua 呼び出し | 戻り値型 | 説明 |
| :--- | :--- | :--- |
| `Physics.CastRay(ox, oy, oz, dx, dy, dz)` | `Entity \| nil` | `Scene::CastRay` を介してワールド座標 `(ox, oy, oz)` から方向 `(dx, dy, dz)` へレイを発射します（最大 1000 単位の Jolt ナローフェーズ判定 + OBB フォールバック）。ヒットした `Entity`、またはヒットなしの場合は `nil` を返します。 |

### `Scene` テーブル

| Lua 呼び出し | 戻り値型 | 説明 |
| :--- | :--- | :--- |
| `Scene.FindEntityByName(name)` | `Entity \| nil` | `NameComponent` を探索し、`name` (`string`) と完全一致するエンティティを返します（見つからない場合は `nil`）。 |

### `Time` & `Game` テーブル

| Lua 式 / 呼び出し | 型 | 説明 |
| :--- | :--- | :--- |
| `Time.deltaTime` | `number` | `ScriptSystemUpdate` の先頭で更新される現在フレームの経過時間（秒）。 |
| `Game.Restart()` | `void` | `Scene::m_requestRestart = true` を設定し、フレーム終端でプレイ前のスナップショットを復元して全スクリプトを再起動します。 |
| `Game.Quit()` | `void` | `Scene::m_requestQuit = true` を設定し、フレーム終端でアプリケーションを安全に終了します。 |

---

## ユーザー型 (Usertypes)

### `Vector3` (`DirectX::XMFLOAT3`)

- **コンストラクタ**: `Vector3.new()` または `Vector3.new(x, y, z)`
- **プロパティ**:
    - `v.x` (`number`)
    - `v.y` (`number`)
    - `v.z` (`number`)

### `TransformComponent`

`self:GetTransform()` から取得します。

| メンバ | 種別 | シグネチャ / 型 | 説明 |
| :--- | :--- | :--- | :--- |
| `position` | プロパティ | `Vector3` | ローカル座標のコピーを取得、または `Vector3` を代入して `isDirty = true` を設定します。 |
| `scale` | プロパティ | `Vector3` | ローカルスケールのコピーを取得、または `Vector3` を代入して `isDirty = true` を設定します。 |
| `SetPosition` | メソッド | `tc:SetPosition(x, y, z)` | `tc.position = {x, y, z}` を設定し、`tc.isDirty = true` をマークします。 |
| `SetScale` | メソッド | `tc:SetScale(x, y, z)` | `tc.scale = {x, y, z}` を設定し、`tc.isDirty = true` をマークします。 |

> **値セマンティクスに関する重要な注意:** `tc.position` や `tc.scale` の読み取りは `Vector3` を **値渡し（コピー）** で返します。そのため、`self:GetTransform().position.x = 5` のように個別フィールドを直接書き換えても一時コピーが変更されるだけで、エンティティは移動しません。必ず `self:GetTransform():SetPosition(x, y, z)` を呼び出すか、`Vector3` 全体を代入（`tc.position = Vector3.new(x, y, z)`）してください。

---

### `Entity` (`Engine::ScriptEntity`)

Lua 上では `Entity` としてバインドされ、各スクリプト環境に `self` として自動注入されます。メソッド呼び出しにはコロン構文（`self:Method(...)` または `other:Method(...)`）を使用します。

#### 識別・有効状態・階層・トランスフォーム

| メソッド | 戻り値 | 説明 |
| :--- | :--- | :--- |
| `entity:GetName()` | `string` | `NameComponent::name` を返します（存在しない場合は `"Unknown"`）。 |
| `entity:IsActive()` | `boolean` | `NameComponent::isActive` を返します（無効なエンティティの場合は `false`）。 |
| `entity:SetActive(active)` | `void` | `NameComponent::isActive` (`boolean`) を設定します。 |
| `entity:GetTransform()` | `TransformComponent&` | エンティティの `TransformComponent` への参照を返します。 |
| `entity:GetForwardVector()` | `Vector3` | エンティティの正規化クォータニオンで `(0, 0, 1)` を回転させた前方ベクトルを返します。 |
| `entity:GetRightVector()` | `Vector3` | エンティティの正規化クォータニオンで `(1, 0, 0)` を回転させた右方ベクトルを返します。 |
| `entity:SetParent(parentEntity)` | `void` | `Scene::ParentEntity` を介してワールド変換を維持したまま `parentEntity` (`Entity`) の子に設定します。 |

#### エンティティ生成と破棄

| メソッド | 戻り値 | 説明 |
| :--- | :--- | :--- |
| `entity:Instantiate(name)` | `Entity` | `name` という名前の空エンティティ（`ID`, `Name`, `Transform`）を生成します。 |
| `entity:InstantiateCube(name)` | `Entity` | 単位キューブの `MeshRendererComponent` を持つエンティティを生成します。 |
| `entity:InstantiateSphere(name)` | `Entity` | 単位球の `MeshRendererComponent` を持つエンティティを生成します。 |
| `entity:Destroy()` / `entity:Destroy(delay)` | `void` | オプションの遅延秒数 `delay`（デフォルト `0.0`）を指定して `Scene::SubmitForDestruction` の破棄キューに登録します。 |

#### コンポーネント所持判定と実行時アタッチ

| メソッド | 戻り値 | 説明 |
| :--- | :--- | :--- |
| `entity:HasMesh()` | `boolean` | `MeshRendererComponent` を持っている場合に `true`。 |
| `entity:HasRigidBody()` | `boolean` | `RigidBodyComponent` を持っている場合に `true`。 |
| `entity:HasLight()` | `boolean` | `LightComponent` を持っている場合に `true`。 |
| `entity:HasAudio()` | `boolean` | `AudioComponent` を持っている場合に `true`。 |
| `entity:HasScript()` | `boolean` | `LuaScriptComponent` を持っている場合に `true`。 |
| `entity:AddMeshRenderer(meshType)` | `void` | `MaterialType::LitColor` で `MeshRendererComponent`（`MeshType.Cube`、`Sphere`、`Capsule`、`Custom`）を追加・更新します。 |
| `entity:AddRigidBody(shape, motion, size, mass, restitution)` | `void` | `RigidBodyComponent` を追加・再構築します。`size` は形状に応じて `radius` (`Sphere`)、`{size, size, size}` (`Box` のハーフエクステントまたは `Mesh` の `colliderScale`)、`radius = size, height = size * 2` (`Capsule`) にマッピングされます。 |
| `entity:AddLuaScript(filepath)` | `void` | 新しい `ScriptInstance` (`filepath`) を `LuaScriptComponent` に追加します。次回の `ScriptSystemUpdate` で初期化されます。 |
| `entity:AddAudioComponent(filepath, is3D, loop, playOnCreate)` | `void` | `filepath` を `AssetType::Audio` としてインポートし、`AudioComponent` を追加・更新します。 |
| `entity:AddSpriteRenderer()` | `void` | 未付与の場合に `SpriteRendererComponent` を追加します。 |
| `entity:AddTextRenderer()` | `void` | 未付与の場合に `TextRendererComponent` を追加します。 |

#### 物理フォースと速度制御

| メソッド | 戻り値 | 説明 |
| :--- | :--- | :--- |
| `entity:ApplyLinearImpulse(x, y, z)` | `void` | 有効なリジッドボディに対してワールド空間の瞬間的な撃力 (`JPH::BodyInterface::AddImpulse`) を加えます。 |
| `entity:SetLinearVelocity(x, y, z)` | `void` | 線速度を直接設定します (`JPH::BodyInterface::SetLinearVelocity`)。 |
| `entity:GetLinearVelocity()` | `Vector3` | Jolt から現在の線速度を取得します（無効時は `{0, 0, 0}`）。 |

#### マテリアル・ライティング・オーディオ・2D UI 操作

| メソッド | 戻り値 | 説明 |
| :--- | :--- | :--- |
| `entity:SetMesh(filepath)` | `void` | `filepath`（例: `"assets/Models/foo.obj?mesh=0"`）をインポートし、`MeshRendererComponent::meshID` に設定します。 |
| `entity:SetAlbedo(filepath)` | `void` | テクスチャ `filepath` をインポートして `albedoMapID` に設定し、`matType` を自動的に `Textured` へ切り替えます。 |
| `entity:SetNormal(filepath)` | `void` | テクスチャ `filepath` をインポートして `normalMapID` に設定し、`matType` を `Textured` へ切り替えます。 |
| `entity:SetORM(filepath)` | `void` | テクスチャ `filepath` をインポートして `ormMapID` に設定し、`matType` を `Textured` へ切り替えます。 |
| `entity:SetLightColor(r, g, b)` | `void` | `LightComponent::color`（`float` RGB）を更新します。 |
| `entity:SetLightIntensity(intensity)` | `void` | `LightComponent::intensity` を更新します。 |
| `entity:PlayAudio()` | `void` | エンティティの `AudioComponent` を再生（再生中の場合は先頭から再再生）します。未ロード時はその場でサウンドハンドルをロードします。 |
| `entity:StopAudio()` | `void` | エンティティの `AudioComponent` の再生を停止します。 |
| `entity:SetAudioVolume(volume)` | `void` | `volume` を `[0.0, 1.0]` にクランプして `AudioComponent::volume` を更新します。 |
| `entity:SetSprite(filepath)` | `void` | テクスチャ `filepath` をインポートし、`SpriteRendererComponent::textureID` に設定します。 |
| `entity:SetSpriteTint(r, g, b, a)` | `void` | `SpriteRendererComponent::tintColor` を更新します。 |
| `entity:SetText(text)` | `void` | `TextRendererComponent::text` を更新します。 |
| `entity:SetTextColor(r, g, b, a)` | `void` | `TextRendererComponent::color` を更新します。 |
| `entity:SetTextPosition(x, y)` | `void` | スクリーンピクセル単位で `TextRendererComponent::position` を更新します。 |
| `entity:SetFont(filepath, size)` | `void` | `fontSize = size` を設定し、JIT フォントアトラスベイク用に `<filepath>?size=<int(size)>` を登録します。 |

---

## バインドされている列挙型

| Lua 列挙型 | 値一覧 |
| :--- | :--- |
| **`MeshType`** | `MeshType.Cube`, `MeshType.Sphere`, `MeshType.Capsule`, `MeshType.Custom` |
| **`RBShape`** | `RBShape.Box`, `RBShape.Sphere`, `RBShape.Capsule`, `RBShape.Mesh` |
| **`RBMotion`** | `RBMotion.Static`, `RBMotion.Dynamic` |
| **`Key`** | 英字: `Key.A` .. `Key.Z`<br/>数字: `Key.Num0` .. `Key.Num9`<br/>矢印キー: `Key.Up`, `Key.Down`, `Key.Left`, `Key.Right`<br/>アクション: `Key.Space`, `Key.Escape`, `Key.Enter`, `Key.Tab`, `Key.Backspace`<br/>修飾キー: `Key.LShift`, `Key.RShift`, `Key.LControl`, `Key.RControl`, `Key.LAlt`, `Key.RAlt` |

> 補足：`Escape` キーの押下はウィンドウ終了処理として `src/main.cpp` (`SDL_SCANCODE_ESCAPE`) でもグローバルに処理されます。
