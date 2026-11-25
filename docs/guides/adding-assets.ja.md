# アセットの追加と管理

Step Engine はプロジェクトの各種コンテンツを `assets/` ディレクトリ配下で整理し、インポートされたすべてのリソースを `enginefiles/AssetRegistry.json` に保存される永続的な 64 ビット `Engine::UUID` で追跡します。

---

## アセットディレクトリの規約と対応フォーマット

CMake ビルド時には、`CopyAssets` ターゲットによってルートの `assets/` フォルダが `${CMAKE_BINARY_DIR}/bin/assets/`（Visual Studio 実行時の `DX11Editor` の作業ディレクトリ）へコピーされます。

> **作業ディレクトリに関する重要なヒント:** Visual Studio から `DX11Editor` を実行する場合、カレントディレクトリは `${CMAKE_BINARY_DIR}/bin`（例: `out/build/x64-Debug/bin`）になります。新しいアセットを追加する際は、Git 管理対象として保持しつつビルド時に自動コピーされるようリポジトリ直下の `assets/` に配置してください（エディタを起動したままファイルを追加する場合は、`assets/` と `bin/assets/` の両方に配置すると即座に認識されます）。

| サブディレクトリ | 対応拡張子 | ドラッグ＆ドロップ Payload | 主な利用先 |
| :--- | :--- | :--- | :--- |
| `assets/Models/` | `.obj` (+ `.mtl`), `.fbx`, `.gltf` (+ `.bin`), `.glb` | `MODEL_FILE` | `MeshManager`, `Scene::InstantiateModel`, `MeshRendererComponent`, `RigidBodyComponent` (`Mesh`) |
| `assets/Textures/` | `.png`, `.jpg`, `.jpeg` | `TEXTURE_FILE` | `TextureManager`, `MeshRendererComponent` (Albedo / Normal / ORM), `SpriteRendererComponent` |
| `assets/Audio/` | `.wav`, `.mp3` | `AUDIO_FILE` | `AudioManager`, `AudioComponent` |
| `assets/Fonts/` | `.ttf` | `FONT_FILE` | `FontManager`, `TextRendererComponent` |
| `assets/Scripts/` | `.lua` | `SCRIPT_FILE` | `LuaScriptComponent`, `ScriptSystem` |
| `assets/Scenes/` | `.json` | *(クリックでロード)* | `SceneSerializer`, `EditorUI` Content Browser |

*(Windows のファイルパスは大文字・小文字を区別しないため `assets/Audio` と `assets/audio` は同様に解決されますが、VFS との互換性を保つためスラッシュ `/` 区切りの一貫した相対パスを使用することを推奨します。)*

---

## 1. 3D モデルのインポート

### エディタからのインポート（ドラッグ＆ドロップ）

1. モデルファイル（および付随する `.mtl` や `.bin` ファイル）を `assets/Models/<YourModel>/` に配置します。
2. `DX11Editor` 下部の **Content Browser** パネルを開き、対象のモデルファイルがあるフォルダへ移動します。
3. **Content Browser** から `.obj`、`.fbx`、`.gltf`、または `.glb` ファイルをドラッグし、**Hierarchy** パネル下部の空白領域へドロップします。
4. `Scene::InstantiateModel` により以下の処理が自動的に実行されます：
    - Assimp（`Triangulate`, `FlipUVs`, `MakeLeftHanded`, `FlipWindingOrder`, `PreTransformVertices`, `CalcTangentSpace`）でモデルを解析します。
    - コンテナファイル (`AssetType::ModelFile`)、`VFSIOSystem` 経由で開かれた `.mtl` / `.bin` サイドカーファイル、および各サブメッシュ (`<filepath>?mesh=<i>`) をレジストリへ登録します。
    - ファイル名（拡張子なし、例: `ABeautifulGame`）を持つルート親エンティティを生成し、各サブメッシュ用の `MeshRendererComponent` を備えた子エンティティ（`ABeautifulGame_mesh_0`, `ABeautifulGame_mesh_1`, ...）を親子付けします。

### メッシュコライダーの追加

インポートしたサブメッシュに物理的な当たり判定を付与するには、以下の手順を行います。

1. **Hierarchy** で対象のサブメッシュ子エンティティを選択します。
2. **Inspector** で **Add Component -> Rigidbody** をクリックします。
3. **Shape** を `Mesh` に変更します：
    - 動かない地形や背景プロップには **Motion Type** として `Static` を選択します（正確な凹形状 `JPH::MeshShape` が構築されます）。
    - 物理演算で動くオブジェクトには **Motion Type** として `Dynamic` を選択します（自動的に凸包 `JPH::ConvexHullShape` が構築されます）。
4. `PhysicsSystem` がエンティティの `MeshRendererComponent::meshID` から自動的に `RigidBodyComponent::meshID` を設定します。必要に応じて **Collider Scale** を調整し、緑色の **Show Wireframe** オーバーレイで形状の一致を確認してください。

### 実行時の Lua からの割り当て

カスタム `MeshRendererComponent` を追加し、サブメッシュインデックス（`?mesh=0`, `?mesh=1` など）を指定して割り当てることも可能です。

```lua
self:AddMeshRenderer(MeshType.Custom)
self:SetMesh("assets/Models/container/model/container.obj?mesh=0")
```

---

## 2. PBR マテリアルとテクスチャの割り当て

`MeshRendererComponent` の **Material Type** を `Textured` に設定すると、メタリック・ラフネス PBR ワークフローが有効になります。

### テクスチャのチャンネルパッキング規約

| スロット | シェーダーレジスタ | 期待されるフォーマット・チャンネル構成 | デフォルトフォールバック（解除 / `0` の場合） |
| :--- | :--- | :--- | :--- |
| **Albedo (Color)** | `t0` | RGBA ベースカラーマップ（`Base Color` ティントと乗算） | `1x1` 純白 `(255, 255, 255, 255)` |
| **Normal Map** | `t1` | タンジェント空間 DirectX 法線マップ（`+Y` 下向き / DirectX 形式） | `1x1` フラット Z-Up `(128, 128, 255, 255)` |
| **ORM Map** | `t2` | **R** = Ambient Occlusion、**G** = Roughness、**B** = Metallic | `1x1` `(255, 128, 0, 255)` (AO=1, Rough=0.5, Metal=0) |

> **Roughness / Metallic スライダーの乗算仕様:** `BasicPS.hlsl` では、サンプリングされた ORM マップの各チャンネルに対してコンポーネント側のスカラースライダー値が乗算されます（`roughness = orm.g * g_Roughness`, `metallic = orm.b * g_Metallic`）。ORM マップの青 (B) チャンネルにメタリック情報が含まれている場合は、金属部分がゼロにならないよう Inspector の **Metallic** スライダーを `0.0` より大きい値（例: `1.0`）に設定してください。

### Inspector での設定手順

1. **Mesh Renderer** コンポーネントを持つエンティティを選択します。
2. **Material Type** を `Textured` に変更します。
3. **Content Browser** から `.png` または `.jpg` ファイルを **Albedo (Color)**、**Normal Map**、**ORM** の各ドロップゾーンへドラッグ＆ドロップします（各スロット横の `X` ボタンをクリックすると 1x1 フォールバックテクスチャに戻ります）。
4. **Roughness** および **Metallic** スライダーを調整して質感を仕上げます。

---

## 3. 2D スプライトと TrueType フォントの追加 (HUD / UI)

### 2D スプライト

1. エンティティを選択します（または Hierarchy で HUD 用の **Empty Entity** を新規作成します）。
2. **Add Component -> Sprite Renderer** をクリックします。
3. **Content Browser** から `.png` / `.jpg` 画像を **Sprite Texture** ドロップゾーンへドラッグします。
4. スクリーン空間のピクセル単位で **Position (X,Y)** と **Size (W,H)**（`(0, 0)` がビューポート左上）、**Pivot (0-1)**（`(0.5, 0.5)` で中央基準）、**Rotation**（度数法）、および **Tint Color** を設定します。

### 動的テキストと `.ttf` フォント

1. `.ttf` フォントファイルを `assets/Fonts/` に配置します（例: `assets/Fonts/FiraCode-VariableFont_wght.ttf`）。
2. エンティティを選択し、**Add Component -> Text Renderer** をクリックします。
3. Inspector の **Font** ドロップダウンから対象の `.ttf` ファイルを選択します。
4. **Font Size** (`8`–`256` px)、**Text**、**Position (X,Y)**、および **Color** を調整します。
    - フォントファイルまたはフォントサイズを変更すると、仮想アセットパス `<filepath>?size=<fontSize>` として `2048x2048` の RGBA グリフアトラス（`stb_truetype`、ASCII `32`–`126`）が自動的に再ベイクされます。

---

## 4. オーディオクリップの追加

1. `.wav` または `.mp3` ファイルを `assets/Audio/` に配置します。
2. **Hierarchy** でエンティティを選択し、**Add Component -> Audio Component** をクリックします。
3. **Audio File** ドロップダウンからクリップを選択するか、**Content Browser** から `.wav` / `.mp3` ファイルをコンボボックス上へドラッグ＆ドロップします。
4. 再生オプションを設定します：
    - **Is 3D**: アクティブカメラのリスナーに対する 3D 距離減衰とパンニングを有効化します。2D の BGM や UI 効果音には無効 (`false`) に設定します。
    - **Looping**: 再生中に連続ループさせます。
    - **Play On Create**: Play モード開始と同時に自動再生します。
    - **Volume**: `0.0`（ミュート）から `1.0`（最大音量）の範囲で調整します。

---

## 5. スカイボックスのカスタマイズ

デフォルトでは、`src/main.cpp` の `LoadCoreAssets()` が DirectX の `+X, -X, +Y, -Y, +Z, -Z` の順序で6枚のキューブマップ面画像をロードします。

1. `assets/Textures/Skybox/right.png` (`+X`)
2. `assets/Textures/Skybox/left.png` (`-X`)
3. `assets/Textures/Skybox/top.png` (`+Y`)
4. `assets/Textures/Skybox/bottom.png` (`-Y`)
5. `assets/Textures/Skybox/front.png` (`+Z`)
6. `assets/Textures/Skybox/back.png` (`-Z`)

6枚の画像はすべて同一のピクセル解像度である必要があります。これら6つの `.png` ファイルを直接差し替えるか、独自の6要素ベクタを `TextureManager::LoadCubemap` および `Renderer::SetSkybox` に渡すことで変更できます。

---

## `AssetRegistry.json` と VFS パッキングの仕組み

- `AssetManager::ImportAsset` が新しいファイルパスを検出すると、ランダムな 64 ビット `UUID` が生成されます。
- シーンの保存時 (`Ctrl + S`) または `DX11Editor` の終了時に、認識されているすべての `(UUID, Filepath, Type)` エントリが `enginefiles/AssetRegistry.json` に書き出されます。
- `assets/Scenes/` 配下に `.json` シーンを保存した後は、`enginefiles/AssetRegistry.json` を **削除しないでください**。シーンファイルはメッシュ、テクスチャ、フォント、オーディオクリップをすべて 64 ビット `UUID` で参照しています。シーンが参照するアセットがレジストリに存在しない場合やディスクから削除されている場合、`SceneSerializer::Deserialize` は現在のシーンを保護したまま読み込みを安全に中断し、不足しているアセット種別を知らせる **Scene Validation Failed** モーダルを表示します。
