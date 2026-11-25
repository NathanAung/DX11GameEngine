# ECS コンポーネントと列挙型

すべての ECS コンポーネントおよび関連する列挙型は、`include/Engine/Components.h` の `namespace Engine` 内で宣言されています。

---

## 列挙型

### `Engine::MeshType`
```cpp
enum class MeshType { Cube, Sphere, Capsule, Custom };
```
`MeshRendererComponent` をアタッチする際に、組み込みプリミティブメッシュまたはカスタムインポートメッシュを選択します（Lua にも `MeshType` として公開されています）。

### `Engine::MaterialType`
```cpp
enum class MaterialType { LitColor, UnlitColor, Textured };
```
- `LitColor` (`0`) — `baseColor`、`roughness`、`metallic` および 1x1 デフォルトテクスチャを用いて Cook-Torrance PBR (`BasicPS.hlsl`) でシェーディングを行います。影を落とします。
- `UnlitColor` (`1`) — `baseColor` を用いた単色フラット描画 (`UnlitPS.hlsl`) を行います。シャドウパスではスキップされます。
- `Textured` (`2`) — `albedoMapID` (`t0`)、`normalMapID` (`t1`)、`ormMapID` (`t2`) をサンプリングし、`baseColor`、`roughness`、`metallic` で乗算する Cook-Torrance PBR (`BasicPS.hlsl`) シェーディングを行います。影を落とします。

### `Engine::CameraControlMode`
```cpp
enum class CameraControlMode { EditorCam = 0, Scripted };
```
`EditorCameraInputSystem` がエンティティの飛行カメラ用キーボード・マウス入力を処理するか (`EditorCam`)、カメラ移動をスクリプト制御に委ねるか (`Scripted`) を決定します。

### `Engine::LightType`
```cpp
enum class LightType : unsigned int
{
    Directional = 0,
    Point       = 1,
    Spot        = 2
};
```
`BasicPS.hlsl` 内の HLSL 構造体 `LightData::type` の整数値と対応しています。

### `Engine::RBShape` & `Engine::RBMotion`
```cpp
enum class RBShape  { Box, Sphere, Capsule, Mesh };
enum class RBMotion { Static, Dynamic };
```
`RigidBodyComponent` のコライダー形状および Jolt のモーション種別・レイヤー挙動（`Layers::NON_MOVING` vs. `Layers::MOVING`）を設定します（Lua にも `RBShape` および `RBMotion` として公開されています）。

---

## 識別・階層・トランスフォーム系コンポーネント

### `IDComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `id` | `uint64_t` | `0` | JSON シーンファイル内で `"EntityID"` として永続化される不変の 64 ビットエンティティ識別子。 |

### `NameComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `name` | `std::string` | `""` | Hierarchy パネルに表示され、`Scene::FindEntityByName` で検索対象となる表示名。 |
| `isActive` | `bool` | `true` | エンティティ全体のマスター有効フラグ。`false` の場合、レンダリング、物理ボディ、レイキャスト、オーディオエミッターがすべて無効化されます。 |

### `RelationshipComponent`
侵入型双方向リストによる階層ノードです。`Scene::ParentEntity` および `Scene::UnparentEntity` を通じて管理されます。

| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `parent` | `entt::entity` | `entt::null` | 親エンティティのハンドル（ルートエンティティの場合は `entt::null`）。 |
| `firstChild` | `entt::entity` | `entt::null` | このエンティティが持つ子リストの先頭要素。 |
| `prevSibling` | `entt::entity` | `entt::null` | 同じ親を共有する前の兄弟エンティティ。 |
| `nextSibling` | `entt::entity` | `entt::null` | 同じ親を共有する次の兄弟エンティティ。 |

### `TransformComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `position` | `DirectX::XMFLOAT3` | `{0.0f, 0.0f, 0.0f}` | `parent` 基準のローカル空間座標（ルートの場合はワールド空間座標）。 |
| `rotation` | `DirectX::XMFLOAT4` | `{0.0f, 0.0f, 0.0f, 1.0f}` | 正規化された `(x, y, z, w)` クォータニオンとして格納されるローカル空間回転。 |
| `scale` | `DirectX::XMFLOAT3` | `{1.0f, 1.0f, 1.0f}` | ローカル空間のスケール係数。 |
| `worldMatrix` | `DirectX::XMFLOAT4X4` | 単位行列 `4x4` | `TransformSystem` によって毎フレーム更新されるキャッシュ済みの行優先ワールド変換行列。 |
| `isDirty` | `bool` | `true` | トランスフォームが変更されたことを示し、`TransformSystem` に子孫への更新伝播を行わせるフラグ。 |

---

## レンダリング・カメラ・ライティング系コンポーネント

### `MeshRendererComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | このコンポーネントのメッシュ描画およびシャドウ投影の有効・無効を切り替えます。 |
| `meshID` | `UUID` | `0` | メッシュアセットの UUID（`primitive://*` または `<model>?mesh=<i>`）。 |
| `albedoMapID` | `UUID` | `0` | `PS t0` にバインドされる PBR アルベド / ディフューズテクスチャの UUID（`0` の場合は `1x1` 白テクスチャを使用）。 |
| `normalMapID` | `UUID` | `0` | `PS t1` にバインドされるタンジェント空間法線マップの UUID（`0` の場合はフラットな `(128, 128, 255)` を使用）。 |
| `ormMapID` | `UUID` | `0` | `PS t2` にバインドされるチャンネルパック済み Ambient Occlusion (`R`) / Roughness (`G`) / Metallic (`B`) テクスチャの UUID。 |
| `matType` | `MaterialType` | `MaterialType::LitColor` | シェーディングモード（`LitColor`、`UnlitColor`、または `Textured`）。 |
| `baseColor` | `DirectX::XMFLOAT4` | `{1.0f, 1.0f, 1.0f, 1.0f}` | RGBA カラー乗数（`UnlitColor` モード時は単色カラー）。 |
| `roughness` | `float` | `0.5f` | スカラーのラフネス値 `[0..1]`（`BasicPS.hlsl` 内で `orm.g` に乗算されます）。 |
| `metallic` | `float` | `0.0f` | スカラーのメタリック値 `[0..1]`（`BasicPS.hlsl` 内で `orm.b` に乗算されます）。 |

### `CameraComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `FOV` | `float` | `DirectX::XM_PIDIV4` (45°) | 垂直視野角（ラジアン）。 |
| `nearClip` | `float` | `0.1f` | 透視投影のニアクリップ面距離。 |
| `farClip` | `float` | `500.0f` | 透視投影のファークリップ面距離（CSM シャドウのテクセル密度を高めるよう調整されています）。 |
| `invertY` | `bool` | `true` | 垂直視点操作の反転設定フラグ。 |

### `ViewportComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `width` | `unsigned` | `1280` | レンダービューポートの幅（ピクセル単位。アスペクト比および 2D UI 平行投影の範囲計算に使用）。 |
| `height` | `unsigned` | `720` | レンダービューポートの高さ（ピクセル単位）。 |

### `EditorCamControlComponent`
エンティティをエディタカメラとしてマークします（Hierarchy リストから非表示になり、Play モードのゲームカメラ自動選択からも除外されます）。

| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `mode` | `CameraControlMode` | `CameraControlMode::EditorCam` | 入力制御モード（`EditorCam` または `Scripted`）。 |
| `moveSpeed` | `float` | `10.0f` | 基本の WASD 移動速度（単位 / 秒）。 |
| `lookSensitivity` | `float` | `0.0025f` | 相対マウス移動 1 ピクセルあたりの回転角（ラジアン）。 |
| `sprintMultiplier` | `float` | `2.0f` | `LShift` 押下中に適用される移動速度倍率。 |
| `yaw` | `float` | `0.0f` | 累積ヨー角（ラジアン、`0` で `+Z` 方向を向きます）。 |
| `pitch` | `float` | `0.0f` | 累積ピッチ角（ラジアン、±89° にクランプされます）。 |

### `LightComponent`
`Directional` および `Spot` ライトの照射方向は、エンティティの `TransformComponent::rotation`（エンティティのクォータニオンで `+Z` を回転させた方向）から算出されます。

| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | ライトの有効・無効を切り替えます。 |
| `color` | `DirectX::XMFLOAT3` | `{1.0f, 1.0f, 1.0f}` | リニア RGB ライトカラー。 |
| `intensity` | `float` | `1.0f` | 放射輝度の乗数。 |
| `type` | `LightType` | `LightType::Directional` | `Directional` (`0`)、`Point` (`1`)、または `Spot` (`2`)。 |
| `range` | `float` | `10.0f` | 減衰カットオフ半径および視錐台カリング用の境界球半径（`Point` / `Spot` 用）。 |
| `spotAngle` | `float` | `DirectX::XM_PIDIV4` | `Spot` ライトのコーン半角カットオフ（ラジアン、`theta <= cos(spotAngle)`）。 |

---

## 物理・スクリプト・オーディオ・2D UI 系コンポーネント

### `RigidBodyComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | `false` に設定すると、`PhysicsSystem` が物理ワールドから Jolt ボディを削除します。 |
| `shape` | `RBShape` | `RBShape::Box` | コライダー形状（`Box`、`Sphere`、`Capsule`、または `Mesh`）。 |
| `motionType` | `RBMotion` | `RBMotion::Static` | `Static` (`NON_MOVING`) または `Dynamic` (`MOVING`)。 |
| `mass` | `float` | `1.0f` | 質量（kg、`motionType == RBMotion::Dynamic` の場合に使用）。 |
| `friction` | `float` | `0.5f` | 表面摩擦係数。 |
| `restitution` | `float` | `0.0f` | 反発係数（弾性）。 |
| `linearDamping` | `float` | `0.0f` | 線速度の減衰係数。 |
| `halfExtent` | `DirectX::XMFLOAT3` | `{0.5f, 0.5f, 0.5f}` | `shape == RBShape::Box` の場合に使用されるローカル半径（ハーフエクステント）。 |
| `radius` | `float` | `0.5f` | `shape` が `Sphere` または `Capsule` の場合に使用される半径。 |
| `height` | `float` | `1.0f` | `shape == RBShape::Capsule` の場合に使用される円柱部分の高さ（`halfHeight = height * 0.5f`）。 |
| `colliderScale` | `DirectX::XMFLOAT3` | `{1.0f, 1.0f, 1.0f}` | `shape == RBShape::Mesh` の場合にエンティティのワールドスケールへ追加乗算されるスケール倍率。 |
| `meshID` | `UUID` | `0` | `RBShape::Mesh` 用の元メッシュ UUID（`0` の場合は `MeshRendererComponent::meshID` から自動設定されます）。 |
| `bodyID` | `JPH::BodyID` | 無効な `BodyID` | 実行時の Jolt ボディハンドル（シリアライズ対象外）。 |
| `bodyCreated` | `bool` | `false` | 実行時の物理ワールド登録状態フラグ（シリアライズ対象外）。 |
| `showWireframe` | `bool` | `true` | Edit モードでこのエンティティを選択した際の緑色ワイヤーフレーム表示を切り替えます。 |

### `ScriptInstance` & `LuaScriptComponent`
`LuaScriptComponent` は `std::vector<ScriptInstance> scripts` を保持しており、1つのエンティティに任意の数の独立したスクリプトをアタッチできます。

| `ScriptInstance` フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `filepath` | `std::string` | `""` | `.lua` スクリプトファイルへの相対パス（JSON では `"Scripts"` 配列にシリアライズされます）。 |
| `env` | `sol::environment` | — | `self` を含むインスタンス固有のサンドボックス Lua 環境（実行時のみ）。 |
| `OnCreate` | `sol::function` | — | Play 開始時または実行時アタッチ時に呼び出されるキャッシュ済みライフサイクルフック。 |
| `OnUpdate` | `sol::function` | — | 毎フレーム `(dt)` 付きで呼び出されるキャッシュ済みフック。 |
| `OnDestroy` | `sol::function` | — | エンティティ破棄時または Play 停止時に呼び出されるキャッシュ済み終了フック。 |
| `OnCollisionEnter` | `sol::function` | — | 物理接触開始時のキャッシュ済みコールバック `(other)`。 |
| `OnCollisionExit` | `sol::function` | — | 物理接触終了時のキャッシュ済みコールバック `(other)`。 |
| `isInitialized` | `bool` | `false` | `env` の構築および `OnCreate` の実行が完了しているかを示すフラグ。 |

### `AudioComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `audioID` | `UUID` | `0` | オーディオクリップアセットの UUID（`.wav` / `.mp3`）。 |
| `is3D` | `bool` | `true` | `true` で 3D 空間減衰を有効化、`false` で空間化を無効化 (`MA_SOUND_FLAG_NO_SPATIALIZATION`) します。 |
| `loop` | `bool` | `false` | `true` の場合、連続ループ再生を行います。 |
| `playOnCreate` | `bool` | `true` | Play モードの最初のフレームで自動的に再生を開始します。 |
| `volume` | `float` | `1.0f` | 音量ゲイン倍率 (`[0.0f .. 1.0f]`)。 |
| `soundHandle` | `void*` | `nullptr` | `AudioSystem` が管理する実行時の不透明な `AudioInstance*` ポインタ（シリアライズ対象外）。 |
| `isPlaying` | `bool` | `false` | 実行時の再生状態フラグ（シリアライズ対象外）。 |

### `SpriteRendererComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | 2D スプライト描画の有効・無効を切り替えます。 |
| `textureID` | `UUID` | `0` | 2D テクスチャの UUID（`0` の場合は `tintColor` で着色された白色クアッドを描画）。 |
| `position` | `DirectX::XMFLOAT2` | `{0.0f, 0.0f}` | スクリーン空間のピクセル座標（`(0, 0)` が左上）。 |
| `size` | `DirectX::XMFLOAT2` | `{100.0f, 100.0f}` | ピクセル単位の幅と高さ。 |
| `pivot` | `DirectX::XMFLOAT2` | `{0.0f, 0.0f}` | 正規化された原点オフセット（`(0, 0)` が左上、`(0.5, 0.5)` が中央）。 |
| `rotation` | `float` | `0.0f` | Z 軸周りの時計回り回転角（ラジアン、Inspector 上では度数法で表示）。 |
| `tintColor` | `DirectX::XMFLOAT4` | `{1.0f, 1.0f, 1.0f, 1.0f}` | RGBA カラーおよび不透明度の乗数。 |

### `TextRendererComponent`
| フィールド | 型 | デフォルト値 | 説明 |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | 2D テキスト描画の有効・無効を切り替えます。 |
| `text` | `std::string` | `"New Text"` | 描画する印字可能 ASCII 文字列 (`32`–`126`)。 |
| `fontID` | `UUID` | `0` | ベイク済み `FontAsset` の UUID (`<filepath>?size=<fontSize>`)。 |
| `fontSize` | `float` | `24.0f` | グリフのピクセル高さ。 |
| `position` | `DirectX::XMFLOAT2` | `{0.0f, 0.0f}` | スクリーン空間ピクセル単位のベースライン開始座標。 |
| `color` | `DirectX::XMFLOAT4` | `{1.0f, 1.0f, 1.0f, 1.0f}` | RGBA テキストカラーおよび不透明度。 |
