# Step Engine

<img src="https://i.imgur.com/8bdSI8v.jpeg" alt="Step Engine" referrerpolicy="no-referrer">

**Step Engine** は、**C++17** と **DirectX 11** を用いてゼロから構築されたモジュラー設計の 3D ゲームエンジンです。単一のコードベースから、用途の異なる2つのターゲットをビルドできます。

- **`DX11Editor`** — **Dear ImGui (Docking)** と **ImGuizmo** を搭載した統合ビジュアル開発環境です。リアルタイムなシーン編集、アセット管理、非破壊プレイテスト、ワンクリックでのゲームパッケージング機能を備えています。
- **`DX11Runtime`** — `DIST_BUILD` プリプロセッサマクロを有効にしてコンパイルされる軽量なスタンドアロン実行ファイルです。エディタ UI を完全に除外し、バイナリ `.pak` 仮想ファイルシステム (VFS) をマウントして直接ゲームプレイを起動します。

---

## 主な機能

| サブシステム | 機能概要 |
| :--- | :--- |
| **フォワード PBR & CSM レンダラー** | DirectX 11 ハードウェアパイプライン、Cook-Torrance マイクロファセット BRDF (GGX、Smith Schlick-GGX、Fresnel-Schlick)、タンジェント空間法線マッピング、3分割カスケードシャドウマップ (3x3 PCF およびテクセルスナッピング対応の 2048x2048 `Texture2DArray`)、最大16個の視錐台カリング対応ライト、スカイボックスキューブマップ、エディタ用オフスクリーンフレームバッファ。 |
| **Entity Component System (ECS)** | **EnTT** によるキャッシュ効率の高いスパースセットレジストリ、14種類の組み込みコンポーネント、侵入型双方向リストによる親子エンティティ階層 (`RelationshipComponent`)、循環参照チェック付きリペアレンティング、遅延エンティティ破棄。 |
| **マルチスレッド物理演算** | `hardware_concurrency - 1` 個のワーカースレッドで動作する **Jolt Physics** 統合。Box、Sphere、Capsule、静的凹形状 `MeshShape`、動的凸包 `ConvexHullShape` コライダー、OBB フォールバック付きレイキャスト、スレッドセーフな衝突コールバックをサポート。 |
| **サンドボックス Lua スクリプティング** | **sol2** でバインドされた組み込み **Lua** ランタイム。エンティティごとの複数スクリプトアタッチ、分離された `sol::environment` サンドボックス、ライフサイクルフック (`OnCreate`、`OnUpdate`、`OnDestroy`)、衝突コールバック (`OnCollisionEnter`、`OnCollisionExit`)、エラー保護付き実行をサポート。 |
| **3D 空間オーディオ** | **miniaudio** バックエンドによる 2D UI・BGM 再生および、アクティブカメラのリスナー姿勢とエンティティのワールド変換に毎フレーム同期する 3D 空間サウンドエミッター。 |
| **2D UI & テキスト描画** | 2D スプライト (`SpriteRendererComponent`) と、`stb_truetype` により実行時に 2048x2048 RGBA グリフアトラスへベイクされる動的 TrueType テキスト (`TextRendererComponent`) のためのスクリーン空間平行投影オーバーレイパス。 |
| **UUID アセット管理・VFS・エクスポート** | 永続的な 64 ビット `Engine::UUID` アセット台帳 (`enginefiles/AssetRegistry.json`)、RapidJSON による4パス・シーンシリアライザ、`PAK2` バイナリアーカイブパッカー / VFS マウンタ、および `ExportedGame/` への自動パッケージング。 |

---

## リポジトリ構成

```text
DX11GameEngine/
├── assets/                 # Game content (Audio, Fonts, Models, Scripts, Textures, Scenes)
├── cmake/                  # Shared CMake helper modules (BuildOptions.cmake)
├── docs/                   # MkDocs technical documentation
├── enginefiles/            # Engine metadata (AssetRegistry.json, Launch.txt)
├── external/imguizmo/      # Vendored ImGuizmo 3D gizmo library
├── include/Engine/         # Engine C++17 public header files
├── shaders/                # HLSL Shader Model 5.0 vertex and pixel shaders
├── src/                    # Application entry point (main.cpp) and Engine implementations
├── tools/                  # Setup and build scripts (SetupProject.bat, BuildGame.bat, VSCode.bat)
├── CMakeLists.txt          # Dual-target CMake configuration (DX11Editor & DX11Runtime)
├── CMakeSettings.json      # Visual Studio CMake presets (x64-Debug, x64-Release)
├── mkdocs.yml              # MkDocs Material configuration
└── vcpkg.json              # vcpkg manifest dependencies
```

---

## 技術スタックと依存ライブラリ

外部ライブラリ（Windows SDK の DirectX ライブラリおよび同梱された `ImGuizmo` を除く）はすべて `vcpkg.json` で宣言され、**vcpkg マニフェストモード**によって自動的に解決されます。

| 依存ライブラリ | 取得元 | エンジン内での役割 |
| :--- | :--- | :--- |
| **Direct3D 11** (`d3d11`, `dxgi`, `d3dcompiler`, `dxguid`) | Windows SDK | GPU レンダリングバックエンドおよび HLSL コンパイラ |
| **SDL2** | `vcpkg` | Win32 ウィンドウ生成、OS イベントループ、高精度タイマー、キーボード・マウス入力 |
| **EnTT** | `vcpkg` | スパースセット方式の Entity Component System (`entt::registry`) |
| **Jolt Physics** | `vcpkg` | 剛体物理シミュレーション、コリジョン形状、ナローフェーズ・レイキャスト |
| **Assimp** | `vcpkg` | カスタム VFS ストリーム対応の 3D モデルインポート (`.obj`, `.fbx`, `.gltf`, `.glb`) |
| **Dear ImGui** (`1.91.8`) | `vcpkg` | ドッキング対応エディタ UI (`dx11-binding`, `sdl2-binding`, `docking-experimental`) |
| **ImGuizmo** | `external/imguizmo/` | 3D ビューポート上のインタラクティブな移動・回転・スケールギズモ |
| **Lua** & **sol2** | `vcpkg` | ゲームプレイスクリプト VM および C++ ユーザー型・列挙型バインディング |
| **RapidJSON** | `vcpkg` | JSON シーンのシリアライズと `AssetRegistry.json` の永続化 |
| **miniaudio** | `vcpkg` | 2D / 3D 空間オーディオ再生およびインメモリ VFS デコード |
| **stb** (`stb_image`, `stb_truetype`) | `vcpkg` | 画像デコード (`RGBA8`) および `.ttf` フォントアトラスのベイク |

---

## クイックスタート・ビルド手順

### 必要環境

- **OS**: Windows 10 または Windows 11 (x64)（DirectX 11 ランタイム搭載）
- **コンパイラ / IDE**: Visual Studio 2022（**C++ によるデスクトップ開発** ワークロード：MSVC v143、Windows 10/11 SDK、Windows 用 C++ CMake ツール）
- **ビルドツール**: CMake 3.20 以降および `vcpkg`（`tools\SetupProject.bat` で自動インストール可能）

### 方法 A: Visual Studio 2022 を使用する（推奨）

1. **CMake と vcpkg のセットアップ（未導入の場合）**:
   リポジトリのルートディレクトリから自動セットアップ用バッチファイルを実行します。
   ```bat
   tools\SetupProject.bat
   ```
   これにより CMake が `%LOCALAPPDATA%\w64cmake` にインストールされ、`%SystemDrive%\vcpkg` に `vcpkg` がクローンおよびブートストラップされます（`%SystemDrive%\vcpkg\scripts\buildsystems\vcpkg.cmake` の存在を確認してください）。

2. **Visual Studio でフォルダを開く**:
   Visual Studio 2022 を起動し、**ファイル -> 開く -> フォルダー...** から `DX11GameEngine` のルートディレクトリを選択します。Visual Studio が自動的に `CMakeLists.txt` と `CMakeSettings.json` を検出します。

3. **CMake の構成と依存パッケージの導入**:
   構成ドロップダウンから `x64-Debug` または `x64-Release` を選択します。`CMakeSettings.json` 内の `cmakeToolchain` が `${env.SystemDrive}/vcpkg/scripts/buildsystems/vcpkg.cmake` を指していることを確認してください。初回構成時には `vcpkg` が `vcpkg.json` に記載されたすべてのパッケージをダウンロードしてビルドします。

4. **両ターゲットのビルド**:
   メニューから **ビルド -> すべてビルド** を選択します。
   - ビルド時にカスタムターゲット `CopyShaders`、`CopyAssets`、`CopyEngineFiles` が実行され、`shaders/`、`assets/`、`enginefiles/` が `${CMAKE_BINARY_DIR}/bin/` にミラーリングされます。
   - `DX11Runtime` のビルド後には `POST_BUILD` ステップが実行され、`bin/RuntimeTemplate/` に `DX11Runtime.exe` と必要なすべての `.dll` ファイルが集約されます。これによりエディタ上の **Export Game** ボタンが利用可能になります。

5. **エディタの起動**:
   スタートアップアイテムとして `DX11Editor.exe` が選択されていることを確認し、`Ctrl + F5`（**デバッグ -> デバッグなしで開始**）を押して起動します。

### 方法 B: コマンドラインからの CMake ビルド

ターミナルから直接（または `tools\BuildGame.bat` を実行して）両方の実行ファイルを構成・ビルドすることもできます。

```powershell
cmake -S . -B out/build/x64-Release -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:SystemDrive/vcpkg/scripts/buildsystems/vcpkg.cmake"

cmake --build out/build/x64-Release --config Release
```

コンパイルされたバイナリは `out/build/x64-Release/bin/Release/` に出力されます（`DX11Editor.exe` および `DX11Runtime.exe`）。

---

## ドキュメント構成

### アーキテクチャ
- [コアエンジンループとライフサイクル](architecture/engine-loop.md) — 初期化フロー、フレーム更新順序、`Edit <-> Play` ステートマシン、デュアルビルドターゲットの仕組み。
- [ECS とエンティティ管理](architecture/ecs.md) — EnTT レジストリ設計、侵入型エンティティ階層、トランスフォーム伝播、遅延破棄。
- [レンダリングパイプラインとシェーダー](architecture/rendering-pipeline.md) — マルチパス DirectX 11 アーキテクチャ、PBR BRDF、カスケードシャドウマップ、スカイボックス、2D UI。
- [サブシステム・物理・オーディオ・VFS](architecture/subsystems.md) — Jolt Physics 統合、Lua サンドボックス、空間オーディオ、UUID アセット追跡、`PAK2` VFS パッケージング。

### API リファレンス
- [コア・Scene・Systems・シリアライズ](api/core-and-scene.md) — `UUID`、`Engine::Math`、`Scene`、`SceneSerializer`、および ECS システム関数群。
- [ECS コンポーネントと列挙型](api/components.md) — `Components.h` で定義されている構造体レイアウト、フィールド型、デフォルト値、列挙型の詳細。
- [Renderer・各種マネージャー・Editor UI](api/managers-and-renderer.md) — `Renderer`、`AssetManager`、`MeshManager`、`ShaderManager`、`TextureManager`、`FontManager`、`PhysicsManager`、`AudioManager`、`InputManager`、`ImGuiManager`、`EditorUI`。
- [Lua スクリプティングバインディング](api/lua-scripting.md) — `ScriptEntity.h` および `Scene::InitializeLuaBindings` に基づく Lua ユーザー型、グローバルテーブル、コールバック、列挙型の完全リファレンス。

### 開発ガイド
- [アセットの追加と管理](guides/adding-assets.md) — 3D モデル、PBR テクスチャマップ、オーディオクリップ、TrueType フォント、スカイボックスのインポート手順。
- [シーンの作成・編集・エクスポート](guides/creating-scenes.md) — ビューポート操作、ギズモ、Hierarchy / Inspector ワークフロー、JSON シーン保存、スタンドアロンゲームのエクスポート。
- [Lua ゲームプレイスクリプトの作成](guides/writing-scripts.md) — スクリプトのライフサイクル、移動制御、スポーン処理、レイキャスト、衝突判定、UI HUD 制御、よくある注意点。
