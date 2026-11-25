# シーンの作成・編集・エクスポート

`DX11Editor` は、3D ワールドの構築、ライティングや物理パラメータの調整、エディタ内でのプレイテスト、およびスタンドアロン実行ファイルのエクスポートを行うためのドッキング対応ワークスペースを提供します。

---

## エディタワークスペースと操作方法

`DX11Editor` の起動時、`EditorUI::SetupDockspace` によって5つのドッキングパネルが初期配置されます。

- **Toolbar** *(上部)* — **Play / Stop** ステート切り替えボタンおよび **Export Game** ボタン。
- **Hierarchy** *(左側)* — シーンのエンティティツリー表示、右クリックメニューからのエンティティ生成、ドラッグ＆ドロップによる親子付け。
- **Scene Viewport** *(中央)* — オフスクリーン DirectX 11 描画ビュー、オブジェクトピッキング、および `ImGuizmo` ハンドル操作。
- **Inspector** *(右側)* — アクティブ状態の切り替え、コンポーネントのプロパティ編集、および **Add / Remove Component** メニュー。
- **Content Browser** *(下部)* — `assets/` をルートとするファイルブラウザ、シーンの読み込み、および各種アセットのドラッグ＆ドロップ元。

### ビューポート操作とショートカットキー

| 入力 | 動作 |
| :--- | :--- |
| **右クリック長押し** *(Scene パネル内)* | カーソルをキャプチャしてフリールックでカメラを回転 |
| **`W` / `A` / `S` / `D`** *(右クリック長押し中)* | 前進・左移動・後退・右移動 |
| **`Left Shift`** *(飛行移動中、または Scene フォーカス中)* | 移動速度に `sprintMultiplier`（デフォルト `2.0x`）を乗算 |
| **マウスホイール** *(Scene フォーカス中)* | 視線方向に沿ってカメラを前後にドリー移動 |
| **左クリック** *(Scene パネル内)* | レイキャストでエンティティを選択（Jolt ナローフェーズ優先、OBB フォールバック） |
| **`Space`** *(Edit モードで Scene フォーカス中)* | `ImGuizmo` モードを切り替え：**Translate** -> **Rotate** -> **Scale** |
| **`Delete`** *(Hierarchy フォーカス中)* | 選択中のエンティティを破棄（全子孫エンティティも一括破棄） |
| **`Ctrl + S`** | 現在のシーンを保存（未保存の場合は **Save Scene As** モーダルを表示） |
| **`Escape`** | アプリケーションを終了 |

---

## ステップ 1: 新規シーンの作成

1. **Content Browser** パネル内の空白部分を右クリックし、**Create New Scene** を選択します。
2. シーン名（例: `Level01`）を入力して **Create** をクリックします。
3. エディタが `Scene::GenerateDefaultSetup` を呼び出し、`assets/Scenes/Level01.json` を保存して Scene タブのタイトルを更新します。新規シーンにはあらかじめ以下の4つのエンティティが配置されます：
    - **`Editor Camera`** *(Hierarchy からは非表示)* — `Edit` モードでの編集作業中に使用する飛行カメラ。
    - **`Main Camera`** — `Play` モード開始時やスタンドアロンビルド実行時に自動的にアクティブ化されるゲームカメラ。
    - **`Sun Light`** — カスケードシャドウマップを駆動する、下向き 45° の白色 `Directional` ライト (`intensity = 5.0`)。
    - **`Ground`** — `(0, -5, 0)` に配置された `20 x 0.1 x 20` の静的ボックスメッシュおよび `RigidBodyComponent`。

---

## ステップ 2: エンティティの配置と階層整理

### 組み込みエンティティの生成

**Hierarchy** パネルの空白部分を右クリックし、**Create New Entity** から生成したい種別を選択します。

- **Empty Entity** — `IDComponent` + `NameComponent` + `TransformComponent` のみを持つ空エンティティ（階層整理用親ノード、スクリプト管理オブジェクト、オーディオエミッター、2D HUD 要素などに適しています）。
- **Camera** — 新しいゲーム用の `CameraComponent` + `ViewportComponent` を作成します。
- **Shapes** -> **Cube** / **Sphere** / **Capsule** — プリミティブ形状の `MeshRendererComponent` を生成します。
- **Lights** -> **Directional Light** / **Point Light** / **Spot Light** — `LightComponent` を生成します（視錐台内の有効なライトが最大16個まで同時描画され、最初の有効な `Directional` ライトが3分割カスケードシャドウを投影します）。

### 親子階層の構築

- **親子付け (Parenting)**: **Hierarchy** 上でエンティティノードをドラッグし、別のエンティティノードの上へドロップします。ワールド空間での位置・回転・スケールはそのまま維持され、`TransformComponent` が親基準のローカル空間へ自動変換されます。
- **ルートへの親子解除 (Unparenting)**: 子エンティティノードをドラッグし、**Hierarchy** パネルのリスト下部の空白領域へドロップします。
- **コンポーネントの削除**: **Inspector** 内の各コンポーネントヘッダー（`Light`, `Rigidbody`, `Mesh Renderer`, `Audio Emitter`, `Lua Scripts`, `Sprite Renderer`, `Text Renderer`）を右クリックし、**Remove Component** を選択します。

---

## ステップ 3: シーンの保存と読み込み

### 保存

- **`Ctrl + S`** を押します。
- まだ一度も保存されていないシーン（起動直後のサンドボックスなど）の場合は **Save Scene As** モーダルが開き、`assets/Scenes/<Name>.json` として書き出されます。
- 以降の **`Ctrl + S`** ではアクティブな `.json` ファイルが上書き保存されると同時に、新しくアタッチされた Lua スクリプトやインポートされたアセットが漏れなく追跡されるよう `enginefiles/AssetRegistry.json` も更新されます。

### 読み込み

1. エディタが **Edit** モードであることを確認します（**Play** モード実行中はシーンを読み込めません）。
2. **Content Browser** で `Scenes` (`assets/Scenes/`) フォルダを開き、`[SCENE] <Name>.json` をクリックします。
3. `SceneSerializer::Deserialize` が現在のシーンをクリアする前にすべてのアセット UUID とスクリプトパスを検証し、全エンティティと階層構造を再構築した上で、参照されているモデルやテクスチャを VRAM にプリロードします。

### シーン JSON 構造の例

シーンデータは可読性の高い JSON 形式で保存されるため、バージョン管理システムでも差分を明確に追跡できます。

```json
{
    "Scene": [
        {
            "EntityID": 2,
            "NameComponent": {
                "Name": "Main Camera",
                "IsActive": true
            },
            "TransformComponent": {
                "Position": [0.0, 0.0, -10.0],
                "Rotation": [0.0, 0.0, 0.0, 1.0],
                "Scale": [1.0, 1.0, 1.0]
            },
            "CameraComponent": {
                "FOV": 0.7853981852531433,
                "NearClip": 0.10000000149011612,
                "FarClip": 500.0,
                "InvertY": true
            }
        }
    ]
}
```

---

## ステップ 4: プレイテストとスタンドアロンゲームのエクスポート

### 非破壊プレイテスト

1. シーン内に有効な **Camera** エンティティ（`Main Camera` など）が少なくとも1つ存在することを確認します。
2. 上部 **Toolbar** の **Play** をクリックします：
    - エディタが現在のワールドをスナップショット (`CopyToBackup()`) し、編集用パネルを非表示にしてゲームカメラへ切り替え、全 Lua スクリプトの `OnCreate()` を実行して Jolt Physics と 3D オーディオのシミュレーションを開始します。
3. **Stop** をクリックすると `OnDestroy()` が呼び出され、実行時の物理ボディやオーディオハンドルが破棄されて、プレイ開始前の正確な状態が復元されます。

### スタンドアロンビルドのエクスポート (`ExportedGame/`)

1. **事前準備**: `${CMAKE_BINARY_DIR}/bin/RuntimeTemplate/` に `DX11Runtime.exe` と必要な `.dll` 群が配置されるよう、事前に一度 `DX11Runtime` ターゲット（または Visual Studio の **ビルド -> すべてビルド**）をビルドしておきます。
2. **Edit** モードの状態で、起動シーンにしたいシーンを保存 (`Ctrl + S`) します。
3. **Toolbar** の **Export Game** をクリックします。
4. `AssetManager::ExportProject` によって `${WorkingDir}/ExportedGame/` が自動的に構築されます：
    ```text
    ExportedGame/
    ├── DX11Runtime.exe          # Standalone game executable (DIST_BUILD)
    ├── *.dll                    # Required runtime DLLs (SDL2, assimp, lua, etc.)
    ├── data.pak                 # Packed PAK2 VFS archive (all assets, .cso shaders, scripts, scenes)
    └── enginefiles/
        └── Launch.txt           # Relative path to the exported startup scene JSON
    ```
5. Windows エクスプローラーで `ExportedGame/` が自動的に開きます。`DX11Runtime.exe` をダブルクリックするとスタンドアロンゲームとしてそのまま起動できます。
