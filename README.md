# Step Engine

<img src="https://i.imgur.com/8bdSI8v.jpeg"/>

## はじめに
Step Engine はゲームエンジンの設計やグラフィックスプログラミングの学習を目的として開発されている、小規模な DirectX 11 ベースのゲームエンジンです。
C/C++ により実装されており、ECS（Entity Component System）、リソース管理、物理演算統合など、実践的なエンジン構成要素を段階的に学べる設計になっています。
現在は基本的なライティング対応、Jolt Physics による物理演算、拡張を前提とした各種サブシステムを備えています。
マテリアル単位での PBR パラメータ（Metallic / Roughness） にも対応しています。

本プロジェクトは開発途中です。

---

## ライセンス

リポジトリを見る前にライセンスをご確認ください。

---

## 使用言語

本プロジェクトは主に以下で実装されています:
- C++
- C
- CMake
- HLSL

備考:
- コアエンジンおよびレンダリングコードは C++17 で記述されています。
- 低レベルのユーティリティや C API は適所で使用されています。
- CMake と vcpkg（manifest モード）でプロジェクトの構成とビルドを行います。
- HLSL は Direct3D 11 の頂点/ピクセルシェーダーに使用されます。

---

## 使用ライブラリとツール

エンジンは複数のオープンソースライブラリとツールに依存しています。バージョン管理は vcpkg manifest モードと CMake で行われます。

- SDL2 — ウィンドウ作成と入力処理（Windows 上）
- DirectX 11 (D3D11) — GPU レンダリングバックエンド
- Jolt Physics — 剛体力学と衝突検出
- Assimp — モデルアセットのインポート（.fbx, .obj 等）
- stb_image — 画像読み込み
- EnTT — ECS（Entity Component System）
- RapidJSON — JSON パーサ（予定）
- Dear ImGui（DX11 バックエンド）— エディタ / UI（予定）
- CMake — ビルドシステム
- Visual Studio / MSVC — Windows 開発環境
- git — バージョン管理

---

## システム — 実装の詳細

以下は主要なエンジンサブシステムの実装と相互作用の概要です。

### SDL を用いたウィンドウ生成と入力処理

- SDL2 がメインアプリケーションウィンドウを作成し、メッセージループを扱います。
- プラットフォーム: 現状は Windows 専用（D3D11 バックエンド）。SDL により Windows 上で安定したキーボード / マウス入力取得ができます。
- SDL の役割:
  - OS ウィンドウの作成とウィンドウイベント（リサイズ、最小化、クローズ）の処理。
  - キーボードとマウスの生イベントの提供。
  - エンジンのメインループで使う高精度タイマーの提供。
- 統合ノート:
  - SDL ウィンドウを作成し、`SDL_SysWMinfo` 経由でネイティブの `HWND` を取得し、D3D11 をその `HWND` を使って初期化します。
  - メインループでは毎フレーム SDL イベントをポーリングし、それらを入力サブシステム（`InputManager`）に渡してエンジンレベルの入力（キー割り当て、マウス差分、スクロール等）にマップします。
  - ウィンドウのリサイズイベントはスワップチェーンのバッファ再構築やレンダーターゲット／ビューポートの再作成を引き起こします。

### グラフィックスレンダリングパイプライン、カメラ、スカイボックス

- レンダラーバックエンド: Direct3D 11 (D3D11)。`Renderer` クラスがデバイス、デバイスコンテキスト、スワップチェーン、レンダーターゲット等をラップしています。
- 一般的なフレームフロー:
  1. CPU 側のシーン更新（トランスフォーム、簡易アニメーション）。
  2. フレーム毎の定数バッファ（カメラ行列、ライト情報）のアップロード。
  3. 描画リストの構築。
  4. 描画コールの実行。
  5. スワップチェーンによる表示。
- カメラ:
  - 古典的なビュー行列と射影行列を使用します。カメラはワールド変換、ビュー行列（look-at）、およびシェーダーで使う逆行列を公開します。
  - 遠近射影（Perspective）とエディタ風のカメラコントローラをサポートします。
- スカイボックス:
  - キューブメッシュとして実装され、専用のスカイボックスシェーダーで描画します。スカイボックスはキューブマップテクスチャを使用し、深度ステートを調整して常に背景として描画されるようにします。

### エンティティ・コンポーネント・システム（ECS）

- EnTT を用いた軽量なコンポーネントベースアーキテクチャを採用しています:
  - エンティティ: ワールド内のオブジェクトを表す小さな整数 ID（ハンドル）。
  - コンポーネント: `Transform`, `MeshRenderer`, `Rigidbody`, `CameraComponent`, `LightComponent` 等の小さな POD 構造体で、EnTT の疎集合に格納されキャッシュフレンドリーに反復されます。
  - システム: 特定のコンポーネントを持つエンティティ群を巡回して作業（レンダリング、物理同期、アニメーション、入力処理）を行う関数群。
- 実装のハイライト:
  - エンティティのライフサイクル: 作成／破棄 API、コンポーネントの追加／削除をサポート。
  - システムはフレーム毎に決まった順序（物理 → アニメーション → スクリプト → レンダリング）で呼び出されます。
- シンプルさと明示性を重視して読みやすく改変しやすい設計です。

### リソースロード（テクスチャとモデル）

- マネージャ:
  - MeshManager: プリミティブ生成と Assimp ベースのモデルインポートを提供。
  - TextureManager: 画像読み込みと SRV 作成、キャッシュ機構を持つ。
  - ShaderManager: シェーダーのコンパイル／バインドと入力レイアウト管理。
- テクスチャ:
  - `stb_image` で PNG/JPG 等の一般フォーマットを読み込みます。
  - 現在は RGBA8 UNORM テクスチャを単一ミップレベルでアップロードします。
- モデル:
  - Assimp でメッシュをインポートし、メッシュごとの頂点属性を解析してエンジンのメッシュバッファへ変換します。
- マテリアル:
  - マテリアルデータ（基本的な PBR パラメータ: metallic, roughness とオプションのテクスチャ SRV）は CPU 側に保持され、描画時に定数バッファとして GPU に渡されます。

### メッシュ生成（ボックス、球、カプセル、モデルメッシュ）

- プリミティブメッシュジェネレータ:
  - ボックス
  - 球
  - カプセル
- モデルメッシュ:
  - Assimp でインポート。頂点属性（位置、法線、タンジェント、UV）をメッシュバッファに格納します。
- メッシュ表現:
  - 頂点バッファとインデックスバッファは D3D11 バッファとして作成され、MeshManager によって管理されます。
  - 各メッシュは描画に必要なメタデータ（インデックス数、トポロジー）を持ち、必要に応じてバウンディングボリュームが利用されます。

### ライティングとマテリアル

- ライトタイプ: ディレクショナル、ポイント、スポット（定数バッファで複数ライトをサポート）。
- マテリアルパラメータ: metallic と roughness。テクスチャ SRV はピクセルシェーダーのスロットにバインド可能。
- PBR に関する注記: 現在のシェーディングはマテリアルパラメータとライト定数を使った前方レンダリング（forward renderer）です。

### 物理（Jolt 統合、剛体生成）

- Jolt Physics を物理サブシステムに統合しています:
  - エンジンは Jolt のワールドを作成し、各シミュレーションティックで進めます。
  - 剛体:
    - `RigidBodyComponent` を持つエンティティから剛体を作成（質量、コリジョン形状、運動タイプ等）。
    - コリジョン形状: プリミティブ（ボックス、球、カプセル）やインポートモデルから生成したメッシュコライダーに対応。
  - 同期:
    - シミュレーション後に動的ボディのトランスフォームを Jolt から読み取り、ECS のコンポーネントに反映します。

### CMake 構成

- プロジェクトは CMake と vcpkg（manifest モード）で依存関係を取得・設定します。
- トップレベルの `CMakeLists.txt`:
  - 実行可能ターゲット `DX11GameEngine` を定義しています。
  - `find_package()` を使って SDL2、Assimp、EnTT、RapidJSON、Dear ImGui、Jolt 等を参照します。
  - Windows 系のシステムライブラリ（`d3d11`, `dxgi`, `d3dcompiler`, `dxguid`）とリンクします。
  - ビルド出力に `shaders/` や `assets/` をコピーするカスタムターゲットを用意しています。

---

## 今後の実装予定

今後の開発予定機能:

- 階層システム（エンティティの親子トランスフォームと伝播）
- エディタ UI（ImGui ベースのシーン編集、マテリアル/アセットインスペクタ）
- オブジェクトピッキングとギズモ（選択・トランスフォームハンドル）
- オーディオシステム（空間オーディオ、再生）
- Lua スクリプティング（高速なゲームロジックの反復開発）
- データ永続化とエクスポート（シーン/アセットのシリアライズ、プロジェクトエクスポートパイプライン）

---

## プロジェクトのセットアップ（Visual Studio）

本プロジェクトは **CMake + vcpkg（manifest モード）** を使用します。すべてのサードパーティ依存は CMake の構成時に自動でダウンロードされます。

### 前提条件

* **Windows 10/11**（DirectX 11 ランタイム）
* **Visual Studio 2022**
* **ワークロード**: デスクトップ開発（C++）
* **コンポーネント**: MSVC、C++ CMake ツール、Git

### 1. vcpkg のインストール

vcpkg がまだインストールされていない場合:

```bash
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
# (オプション推奨)
.\vcpkg integrate install
```

`C:\vcpkg\scripts\buildsystems\vcpkg.cmake` が存在することを確認してください。

### 2. リポジトリをクローン

```bash
git clone https://github.com/NathanAung/DX11GameEngine.git
```

### 3. Visual Studio でプロジェクトを開く

1. Visual Studio を起動します。
2. **ファイル → フォルダーを開く** を選択します。
3. クローンした `DX11GameEngine` フォルダーを開きます。
4. Visual Studio が `CMakeLists.txt` を自動検出します。

### 4. CMake の設定（Visual Studio UI）

1. **プロジェクト → CMake 設定** を開きます。
2. 設定（例: `x64-Debug`）を選択します。
3. 以下の CMake 変数を設定します:
   * **Name**: `CMAKE_TOOLCHAIN_FILE`
   * **Value**: `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
4. 設定を保存します。
5. Visual Studio が CMake を構成し、vcpkg を呼び出して依存関係を取得します。
   * 最初の構成は数分かかる場合があります。

### 5. ビルドと実行

1. Visual Studio ツールバーで:
   * **構成**: `Debug` または `Release`
   * **アーキテクチャ**: `x64`
   * **スタートアップターゲット**: `DX11GameEngine`
2. **ビルド**: ビルド → すべてビルド
3. **実行**: デバッグ → デバッグなしで開始（Ctrl + F5）

---

## リリース
v0.8 までに本エンジンの現在の機能を紹介するため、複数のデモを公開しています。

* **Engine Demo**：レンダリング、ライティング、物理演算など、エンジンの基本的な機能を実演します。　<br> [**[こちらからダウンロード]**](https://github.com/NathanAung/DX11GameEngine/releases/tag/v0.8-demo)
* **3D Model Demo**：詳細な 3D モデルの読み込みおよびレンダリングを実演します。　<br> [**[こちらからダウンロード]**](https://github.com/NathanAung/DX11GameEngine/releases/tag/v0.8-3d-model)
* **Game Demo**：エンジンの物理演算機能を実演します。カメラを操作し、ボールを発射して箱の壁を破壊することができます。　<br> [**[こちらからダウンロード]**](https://github.com/NathanAung/DX11GameEngine/releases/tag/v0.8-game)
* **Physics Demo**：ガルトンボードのシミュレーションを通して、エンジンの物理演算機能を実演します。　<br> [**[こちらからダウンロード]**](https://github.com/NathanAung/DX11GameEngine/releases/tag/v0.8-physics)

---

## デモ動画
### ライティングデモ（クリックで再生）
[![Lighting Demo](https://i.imgur.com/0RIBnX6.jpeg)](https://youtu.be/dPvAgLWRbKk)
### ガルトンボード物理デモ（クリックで再生）
[![Galton Board Physics Demo](https://i.imgur.com/Bj7wcyO.jpeg)](https://youtu.be/JmqsDxMvVeI)
### Wall Smasher ゲームデモ（クリックで再生）
[![Wall Smasher Game Demo](https://i.imgur.com/jCIpugV.jpeg)](https://youtu.be/DlM2aOIT1i8)
### 3D モデルデモ（クリックで再生）
[![3D Model Demo](https://i.imgur.com/4AC6h1Z.jpeg)](https://youtu.be/XPVFZV_9ea4)

## ギャラリー
### PBR ライティング
- メタリック／ラフネスによる PBR 表現
<img src="https://i.imgur.com/FqLpL3O.jpeg"/>

- ディレクショナル、スポット、ポイントライト
<img src="https://i.imgur.com/fT8hAZw.jpeg"/>

### 3D モデルの読み込みとレンダリング
<img src="https://i.imgur.com/qVgbAAt.jpeg"/>

<img src="https://i.imgur.com/nliMOQQ.jpeg"/>

--- 
