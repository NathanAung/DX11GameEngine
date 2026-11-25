# Step Engine

[English](README.md) | **日本語**

<img src="https://i.imgur.com/8bdSI8v.jpeg"/>

## 概要

**Step Engine** は、**C++17** と **DirectX 11** を用いてゼロから構築されたカスタム 3D ゲームエンジンです。リアルタイムレンダリングおよびゲームプレイ開発のための実践的かつモジュール化されたアーキテクチャとして設計されており、本格的な **ドッキング対応エディタ** と **スタンドアロン・ゲームランタイム** の両方を備えています。

主な機能として、タンジェント空間法線マッピングと 3 カスケードシャドウマップを備えた DirectX 11 フォワード PBR レンダラー、侵入型エンティティ階層を持つ EnTT ベースの ECS、マルチスレッド対応の Jolt Physics 統合、`sol2` を介したサンドボックス化 Lua スクリプティング、3D 空間オーディオ、2D スプライト／フォント HUD レンダリング、UUID ベースのアセットパイプラインとバイナリ `.pak` 仮想ファイルシステム（VFS）、ワンクリックでのスタンドアロンゲームエクスポート機能を搭載しています。

---

## ライセンス

本リポジトリを閲覧・利用する前に、[LICENSE](LICENSE) ファイルをご確認ください。All rights reserved — 本リポジトリは閲覧のみを目的として提供されています。

---

## リリース

Step Engine の最新リリースはこちらからダウンロードできます:
- [v1.0 リリース](https://github.com/NathanAung/DX11GameEngine/releases/tag/v1.0-release)

---

## ドキュメント

Step Engine のドキュメントはこちらから閲覧できます:
- [Step Engine ドキュメント](https://nathanaung.github.io/DX11GameEngine/)

---

## 技術スタック

### 使用言語
- **C++17** — コアエンジン、レンダリング、物理、エディタ、ランタイムアーキテクチャ
- **HLSL (Shader Model 5.0)** — Direct3D 11 頂点／ピクセルシェーダー
- **Lua** — ゲームプレイスクリプティングおよびランタイムのエンティティ制御
- **CMake / Batch / PowerShell** — ビルド設定、依存関係解決、環境セットアップスクリプト

### ライブラリとツール
すべてのサードパーティ依存関係は **CMake** と **vcpkg（manifest モード）**（`vcpkg.json`）で管理されており、ImGuizmo は `external/` 配下に同梱されています:
- **DirectX 11 (D3D11 / DXGI / D3DCompiler)** — GPU レンダリングバックエンド
- **SDL2** — ウィンドウ管理、OS イベントループ、高精度タイマー、キーボード／マウス生入力
- **EnTT** — Entity Component System（疎集合レジストリ）
- **Jolt Physics** — マルチスレッド剛体力学、衝突検出、レイキャスト
- **Assimp** — カスタム VFS ストリーム対応の 3D モデルインポート（`.obj`, `.fbx`, `.gltf`, `.glb`）
- **Dear ImGui (DX11 + SDL2 + Docking)** & **ImGuizmo** — エディタ UI および 3D トランスフォームギズモ
- **Lua** & **sol2** — 組み込みスクリプトランタイムおよび C++/Lua バインディング
- **RapidJSON** — シーンおよびアセットレジストリの JSON シリアライズ／デシリアライズ
- **miniaudio** — 2D および 3D 空間オーディオエンジン
- **stb (`stb_image`, `stb_truetype`)** — テクスチャ読み込みおよび TrueType フォントアトラスのベイク
- **CMake**、**vcpkg**、**Visual Studio 2022 (MSVC)** — ビルドツールチェーン

---

## アーキテクチャと実装の詳細

### レンダリングパイプライン

`Renderer` は DirectX 11 デバイス、イミディエイトコンテキスト、ダブルバッファのスワップチェーン、深度ステンシルバッファ、定数バッファ、および各種パイプラインステートをカプセル化しています。

- **エディタ版とスタンドアロン版のレンダーターゲット**:
  - **エディタビルド** では、3D シーンをオフスクリーンの **Render-to-Texture フレームバッファ**（`RTV` + `DSV` + `SRV`）へ描画します。フレームバッファは ImGui の Scene パネルに合わせて動的にリサイズされ、その後 ImGui をスワップチェーンのバックバッファへ合成します。
  - **スタンドアロンビルド** では、スワップチェーンのバックバッファへ直接描画します。
- **フレームパスの実行順序**:
  1. **カスケードシャドウパス** — 深度専用パス（`ShadowVS.hlsl`、ピクセルシェーダー無効）で、有効なライト対応ジオメトリを 3 カスケードのシャドウマップ配列の各スライスに描画します。
  2. **メイン 3D ジオメトリパス** — メインのフレームバッファ／バックバッファを復元し、カメラ・ライト・シャドウの定数バッファをアップロードして、PBR または Unlit シェーダーでエンティティを描画します。
  3. **デバッグコライダーパス（Edit モードのみ）** — 現在選択中のエンティティのコライダーを緑色のワイヤーフレームメッシュで描画します。
  4. **スカイボックスパス** — ビュー行列から平行移動成分を除去し、深度テストを用いて 6 面キューブマップをキューブメッシュに描画することで、未描画の背景ピクセルのみを埋めます。
  5. **2D UI / HUD パス** — アルファブレンドを有効化し、深度テスト／書き込みを無効化したうえで、正射影行列によるスクリーンスペースピクセル座標で 2D スプライトと動的テキストを描画します。
- **メッシュ・テクスチャ・シェーダー・フォント**:
  - `MeshManager` は 32 ビットインデックス付き D3D11 頂点／インデックスバッファ（`Position`, `Normal`, `TexCoord`, `Tangent`）を管理します。プロシージャルプリミティブ（`Cube`, `Sphere`, `Capsule`, `Quad`）の生成と Assimp による外部モデルのインポートを行い、物理コライダー用に CPU 側の頂点／インデックスもキャッシュします。
  - `TextureManager` は `stb_image` で 2D 画像と 6 面キューブマップを D3D11 SRV として読み込み、シェーダーステートの漏れを防ぐ 1×1 フォールバックテクスチャを生成します。
  - `ShaderManager` はエディタモードで HLSL シェーダーをバイトコードへコンパイルし（パッキング用に `.cso` をディスクへキャッシュ）、スタンドアロンビルドでは事前コンパイル済み `.cso` バイナリを VFS から直接読み込みます。
  - `FontManager` は `stb_truetype` を用いて `.ttf` フォントを 2048×2048 の RGBA テクスチャアトラス（ASCII 32–126）にベイクします。グリフのカバレッジをアルファチャンネルに格納することで、`SpritePS.hlsl` 側で動的に文字色を変更できます。

---

### PBR ライティング・法線マッピング・カスケードシャドウマップ

#### 物理ベースシェーディング（`BasicPS.hlsl`）
サーフェスシェーディングにはフォワード方式の **Cook-Torrance マイクロファセット BRDF** を採用しています:
- **法線分布関数 (`D`)**: GGX / Trowbridge-Reitz
- **幾何減衰項 (`G`)**: Schlick-GGX を用いた Smith 法
- **フレネル項 (`F`)**: メタリックワークフローの基本反射率 `F0 = lerp(0.04, albedo, metallic)` を用いた Fresnel-Schlick 近似
- **エネルギー保存**: `kD = (1 - F) * (1 - metallic)` に Lambert 拡散項（`kD * albedo / PI`）と AO で変調したアンビエント項を組み合わせています。

#### PBR マテリアルと法線マッピング
- `MeshRendererComponent` は `LitColor`、`UnlitColor`、`Textured` の 3 つのマテリアルモードをサポートします。
- テクスチャ付き PBR マテリアルでは、スカラー乗数（`baseColor`, `roughness`, `metallic`）に加えて 3 つの専用テクスチャスロットをバインドします:
  - `t0`: **アルベドマップ（Albedo Map）**
  - `t1`: **タンジェント空間法線マップ（Tangent-Space Normal Map）**
  - `t2`: **ORM マップ**（R = Ambient Occlusion、G = Roughness、B = Metallic）
- **タンジェント空間法線マッピング**: `BasicVS.hlsl` で頂点法線とタンジェントをワールド空間へ変換し、`BasicPS.hlsl` で **グラム・シュミットの直交化**（`T = normalize(T - dot(T, N) * N)`）によってタンジェントを法線に対して再直交化します。さらに `cross(N, T)` でバイタンジェントを求め、構築した `TBN` 行列を通してサンプリングした法線をタンジェント空間からワールド空間へ変換します。

#### マルチライトのカリングとソート
- **ディレクショナル**、**ポイント**、**スポット** の各ライト（`LightComponent`）に対応し、1 フレームあたり最大 **16 個のアクティブライト**（`MAX_LIGHTS`）を `CB_Light`（`b3`）で GPU へ送信します。
- ポイントライトおよびスポットライトでは逆二乗減衰と滑らかな距離カットオフを組み合わせ、スポットライトではコーン角による判定を行います。
- GPU へのアップロード前に、`RenderSystem::DrawEntities` がアクティブカメラからワールド空間の `DirectX::BoundingFrustum` を生成し、範囲球との交差判定で視錐台外のポイント／スポットライトをカリングしたうえで、ディレクショナルライトを最優先しつつ残りのライトをカメラからの距離の二乗でソートします。

#### カスケードシャドウマップ（CSM）
- **シャドウマップ配列**: 3 スライスの **2048×2048 `Texture2DArray`** を使用し、深度描画用に各カスケード個別の `D32_FLOAT` Depth-Stencil View、サンプリング用に統合された `R32_FLOAT` Shader Resource View（`t3`）を割り当てています。
- **視錐台分割と正射影フィッティング**: `Renderer::CalculateCascadeMatrices` がカメラ視錐台をクリップ範囲（`near`〜`far`）の **5%**、**20%**、**100%** で分割し、各スライスのワールド空間における 8 頂点を抽出して境界球で囲み、カメラ背後のキャスターも捉えられるよう Z 軸パディング（`500.0f`）を加えたタイトな正射影ライト行列を構築します。
- **シマー抑制とフィルタリング**:
  - **テクセルスナップ（Texel Snapping）**: ライト空間の原点をシャドウテクセル単位に丸めることで、カメラ移動時のシャドウエッジのちらつき（シマー）を防止します。
  - **ハードウェア深度バイアス**: `SlopeScaledDepthBias = 1.5f` と最小限のシェーダーバイアス（`0.0001f`）を組み合わせてシャドウアクネを軽減しています。
  - **3×3 PCF（Percentage-Closer Filtering）**: カメラ空間の `viewZ` と `cascadeSplits` から該当カスケードを選択して 3×3 サンプリングを行い、ディレクショナルライトの影のエッジを滑らかにします。

---

### エンティティ・コンポーネント・システム（ECS）

シーンワールドは **EnTT**（`Scene::registry`）で駆動されており、軽量な POD コンポーネントをキャッシュ効率の高い疎集合に格納しています。

#### 主なコンポーネント
| コンポーネント | 役割 |
| :--- | :--- |
| `IDComponent` / `NameComponent` | シリアライズ用の安定した 64 ビット ID、表示名、エンティティ全体の `isActive` トグル |
| `TransformComponent` | ローカルの `position`、クォータニオン `rotation`、`scale`、キャッシュ済み `worldMatrix`、`isDirty` フラグ |
| `RelationshipComponent` | 侵入型階層リンク（`parent`, `firstChild`, `prevSibling`, `nextSibling`） |
| `MeshRendererComponent` | メッシュ UUID、Albedo/Normal/ORM テクスチャ UUID、`MaterialType`、カラーティント、Roughness、Metallic |
| `LightComponent` | ディレクショナル／ポイント／スポットライト種別、色、強度、範囲、コーン角 |
| `CameraComponent` / `ViewportComponent` / `EditorCamControlComponent` | カメラの FOV／クリップ面、ビューポート寸法、エディタフライカメラのパラメータ |
| `RigidBodyComponent` | コライダー形状・寸法、Static/Dynamic 運動種別、質量、摩擦、反発係数、減衰、Jolt `BodyID` |
| `LuaScriptComponent` | アタッチされた `ScriptInstance` のリスト（独立した `sol::environment` + キャッシュ済みコールバック） |
| `AudioComponent` | オーディオアセット UUID、2D/3D 空間化トグル、ループ／生成時再生フラグ、音量、ランタイムハンドル |
| `SpriteRendererComponent` / `TextRendererComponent` | スクリーンスペースの 2D スプライト（位置、サイズ、ピボット、回転、ティント）および TTF テキスト要素 |

#### エンティティ階層とトランスフォーム伝播
- **侵入型双方向連結ツリー**: `RelationshipComponent` により、エンティティごとの子配列のヒープ確保なしに任意の親子階層を表現します。
- **安全な親子付け替え（Reparenting）**: `Scene::ParentEntity` は祖先を辿る **循環参照チェック** を行い、元の親から切り離したうえで `newLocal = childWorld * Inverse(parentWorld)` を計算してローカル TRS へ分解することでワールド変換を保持します。`Scene::UnparentEntity` はワールド行列をローカル TRS へ引き上げます。
- **階層更新**: `TransformSystem` はルートエンティティ（`parent == entt::null`）から開始し、兄弟チェーンを辿りながら `World = Local * ParentWorld` を再帰的に伝播させ、祖先の変更時にダーティフラグを子孫へ引き継ぎます。
- **カスケード破棄と遅延破棄**: `Scene::DestroyEntity` は子孫を再帰的に破棄し、Jolt 物理ボディの解除、Lua の `OnDestroy` 呼び出し、兄弟リンクの修復を行います。ゲームプレイ中は `Scene::SubmitForDestruction(entity, delay)` で遅延破棄キューに登録し、更新ステップ末尾で安全に処理することでイテレータ無効化を防ぎます。

#### フレーム更新順序
各フレームでは以下の決定的な順序でシステムが実行されます:
1. `PhysicsSystem`（ボディ生成／破棄 + Play モードでは Jolt シミュレーション、Edit モードではギズモ同期）
2. `ScriptSystemUpdate` *（Play モード）*
3. `AudioSystem` *（Play モード）*
4. `Scene::ProcessDestructionQueue` *（Play モード）*
5. `EditorCameraInputSystem` *（Edit モード）*
6. `TransformSystem`（ローカルからワールドへの階層行列カスケード）
7. `CameraMatrixSystem`（アクティブカメラのビュー／射影行列アップロード）

---

### 物理演算

`PhysicsManager` は **Jolt Physics** を統合しており、マルチスレッドの `JobSystemThreadPool`（`hardware_concurrency - 1` ワーカースレッド）、128 MB の `TempAllocatorImpl`、最大 10,240 ボディおよび 65,536 コンタクト制約の構成で動作します。

- **ブロードフェーズとコリジョンレイヤー**: 静的（Static）レイヤーと動的（Dynamic）レイヤーを分離し、静的ボディは動的ボディとのみ衝突し、動的ボディは双方と衝突するようフィルタリングします。
- **コリジョン形状とキャッシュ**:
  - **Box**、**Sphere**、**Capsule**、**Mesh** コライダーに対応します。
  - メッシュコライダーは、静的ボディにはインデックス付き凹面 `JPH::MeshShape`、動的ボディには `JPH::ConvexHullShape` を自動生成し、生成済み形状をメッシュ UUID ごとにキャッシュします（`m_meshShapeCache`）。
  - エンティティのワールドスケール（メッシュコライダーの場合は `colliderScale` も乗算）は `JPH::ScaledShape` で適用します。
- **ECS との双方向同期**:
  - **Play モード**: デルタタイムを `1/30 秒` 以下にクランプし、トンネリング抑制のため 4 サブステップで Jolt を更新した後、ワールド空間の変換を ECS のローカル変換へ逆算して反映します（親も物理ボディの場合は Jolt から直接現在フレームの姿勢を取得し、1 フレーム遅延を防止します）。
  - **Edit モード**: Inspector や ImGuizmo で変更されたワールド変換を Jolt 側へプッシュし、ボディのテレポートや運動量のリセットを行い、スケール・形状・質量・減衰が変更された場合はボディを自動再構築します。
- **レイキャスト**: `Scene::CastRay` はまず Jolt のナローフェーズレイキャストを実行し、ヒットしなかった場合はライトや未コライダーのメッシュなど非物理エンティティ向けに OBB（有向境界ボックス）交差判定へフォールバックします。
- **スレッドセーフな衝突コールバック**: `PhysicsContactListener` は Jolt ワーカースレッドからの `OnContactAdded` / `OnContactRemoved` イベントをミューテックス保護されたキューに蓄積し、`Update()` 後にメインスレッドで順次フラッシュして、対象エンティティの Lua スクリプトの `OnCollisionEnter(other)` / `OnCollisionExit(other)` を呼び出します。

---

### エンジンエディタ

エディタは **Dear ImGui (Docking)** と **ImGuizmo** で構築されており、スタンドアロンゲームビルドではプリプロセッサマクロ `DIST_BUILD` によって自動的に除外されます。

#### ドッキングパネルとワークフロー
- **Toolbar**: **Play / Stop** 切り替えボタンと **Export Game** ボタンを配置しています。
- **Scene Viewport**: オフスクリーンフレームバッファの SRV を表示し、アクティブカメラのビューポートとフレームバッファを動的にリサイズします:
  - **ImGuizmo 3D マニピュレーション**: 移動・回転・スケール（`Space` キーで切替）に対応し、親を持つエンティティでも操作後のワールド行列をローカル空間へ自動逆変換します。
  - **オブジェクトピッキング**: 左クリックで `Scene::CastRay` を介したスクリーン・ワールド変換レイキャストを実行します。
  - **フライカメラ操作**: 右クリックでカーソルを相対マウスモードにキャプチャし、WASD + Shift ダッシュ + マウスルック、およびスクロールホイールでの移動が可能です。ImGui と `InputManager` 間で入力を適切にルーティングします。
- **Hierarchy**: エンティティツリーを表示し、右クリックからの生成（空エンティティ、カメラ、プリミティブ、ライト）、削除（`Delete` キーまたはコンテキストメニュー）、ドラッグ＆ドロップによる親子付け・解除、3D モデルのシーン直接ドロップ（`Scene::InstantiateModel` が親エンティティとサブメッシュの子エンティティを自動生成）をサポートします。
- **Inspector**: アタッチされた全コンポーネントのリアルタイム編集、有効／無効トグル、コンポーネント追加／削除メニュー、オイラー角キャッシュによるジンバルジッターを抑えたクォータニオン回転編集、物理プロパティの即時同期、さらに PBR テクスチャ・Lua スクリプト・オーディオ・`.ttf` フォントのドラッグ＆ドロップ領域を提供します。
- **Content Browser**: `assets/` ディレクトリをナビゲートし、クリックでの `.json` シーン読み込み（VRAM プリロード付き）、右クリックからの **Create New Scene**、モデル・テクスチャ・スクリプト・オーディオのドラッグソースとして機能します。`Ctrl+S` でのクイック保存および保存・新規作成・エラー表示用のモーダルダイアログも備えています。

#### エディタステートマシン（`Edit` <-> `Play`）
`EditorUI` は 2 つの状態（`Edit` と `Play`）を管理し、エディタ内での非破壊プレイテストを実現します:
- **Play モード開始時（`Edit -> Play`）**:
  1. `Scene::CopyToBackup()` が全エンティティとコンポーネントを EnTT ハンドルごと `m_backupRegistry` へスナップショット保存します。
  2. `ScriptSystemInit` が全 Lua 環境を初期化し、`OnCreate()` を呼び出します。
  3. アクティブレンダーカメラをキャッシュ済みのエディタカメラから最初の有効なゲームカメラへ切り替えます。
  4. エディタパネル（Hierarchy、Inspector、Content Browser）を非表示にしてビューポートをゲーム画面とし、`Update()` で物理シミュレーション・Lua 更新・オーディオを有効化します。
- **Play モード終了時（`Play -> Edit`）**:
  1. `ScriptSystemShutdown` が `OnDestroy()` を呼び出し、全スクリプトインスタンスをリセットします。
  2. `Scene::RestoreFromBackup()` がランタイムの Jolt ボディと `miniaudio` サウンドハンドルをすべて破棄し、ライブレジストリをクリアしてプレイ前のクリーンなスナップショットを復元します（ランタイムハンドルは無効化され、各サブシステムが再初期化されます）。
  3. ビューポートをエディタカメラに戻し、全エディタパネルを再表示します。

---

### Lua スクリプティングシステム

ゲームプレイロジックは **Lua** で記述し、**sol2** を通じてエンジンへ統合しています。

- **スクリプト単位のサンドボックス化と複数スクリプト対応**: `LuaScriptComponent` は複数の `ScriptInstance` を保持できるため、1 つのエンティティに複数のスクリプトをアタッチできます。各スクリプトは `ScriptEntity` ラッパーを `self` としてバインドした独立の `sol::environment` 上で実行され、エンティティ間のグローバル名前空間衝突を防止します。

- **ライフサイクルと物理コールバック**:
  - `OnCreate()` — Play モード開始時（またはランタイム中にスクリプトが動的アタッチされた最初のフレーム）に呼び出されます。
  - `OnUpdate(dt)` — Play モード中、毎フレームデルタタイム付きで呼び出されます。
  - `OnDestroy()` — エンティティ破棄時または Play モード終了時に呼び出されます。
  - `OnCollisionEnter(other)` / `OnCollisionExit(other)` — Jolt の物理接触イベントを受け、衝突相手の `ScriptEntity` とともにメインスレッドで呼び出されます。

- **安定性とエラー分離**: スクリプトの読み込みには `safe_script` / `safe_script_file`、実行には `sol::protected_function` を使用し、構文エラーや実行時エラーが発生してもエンジンをクラッシュさせず `stderr` へ出力します。また、エンティティ一覧のスナップショット取得とコールバックのスタックコピーにより、スクリプト実行中の新規エンティティ生成による EnTT プール再確保に対しても安全に動作します。

- **公開スクリプティング API**:
  - **`Entity`（`self`）**:
    - トランスフォーム操作（`GetTransform`, `GetForwardVector`, `GetRightVector`）
    - アクティブ状態（`IsActive`, `SetActive`）
    - 階層操作（`SetParent`）
    - 生成と遅延破棄（`Instantiate`, `InstantiateCube`, `InstantiateSphere`, `Destroy(delay)`）
    - ランタイムでのコンポーネント確認・追加（`AddMeshRenderer`, `AddRigidBody`, `AddLuaScript`, `AddAudioComponent`, `AddSpriteRenderer`, `AddTextRenderer`）
    - PBR マテリアル／メッシュ設定（`SetMesh`, `SetAlbedo`, `SetNormal`, `SetORM`）
    - ライティング（`SetLightColor`, `SetLightIntensity`）
    - 物理操作（`ApplyLinearImpulse`, `SetLinearVelocity`, `GetLinearVelocity`）
    - オーディオ（`PlayAudio`, `StopAudio`, `SetAudioVolume`）
    - 2D HUD（`SetSprite`, `SetSpriteTint`, `SetText`, `SetTextColor`, `SetTextPosition`, `SetFont`）
  - **グローバルテーブル**:
    - `Input`（`IsKeyDown`, `IsKeyPressed`, `IsKeyReleased` + `Key` 列挙体）
    - `Physics.CastRay(...)`
    - `Scene.FindEntityByName(name)`
    - `Time.deltaTime`
    - `Game.Restart()` / `Game.Quit()`

```lua
-- 例: キー入力で音声付きの物理スフィアを生成
function OnUpdate(dt)
    if Input:IsKeyPressed(Key.Space) then
        local ball = self:Instantiate("Bouncing Ball")
        local pos = self:GetTransform().position
        ball:GetTransform():SetPosition(pos.x, pos.y + 2.0, pos.z)

        ball:AddMeshRenderer(MeshType.Sphere)
        ball:AddRigidBody(RBShape.Sphere, RBMotion.Dynamic, 0.5, 1.0, 0.8)
        ball:AddAudioComponent("assets/audio/spawn.mp3", true, false, true)
    end
end
```

---

### アセット管理・VFS・ゲームエクスポート

#### UUID とアセットレジストリ
- すべてのアセット（メッシュ、モデルファイル、テクスチャ、シェーダー、オーディオ、フォント、Lua スクリプト、シーン）はランダム生成される 64 ビットの **`Engine::UUID`** で一意に識別されます。
- `AssetManager` は `UUID -> AssetMetadata`（`filepath`, `AssetType`, `isLoaded`）の中央レジストリを管理し、パス単位での重複インポートを防ぎつつ、仮想／プロシージャル識別子も扱います。
- **永続アセット台帳**: エディタモードでは RapidJSON を用いて `enginefiles/AssetRegistry.json` にレジストリを保存・復元するため、セッションをまたいでもプロシージャルアセットおよび物理アセットの UUID が一貫して維持されます。
- **JIT（Just-In-Time）ローディングとプリロード**: `RenderSystem` は未ロードのメッシュ・PBR テクスチャ・フォントアトラスを UUID メタデータからオンデマンドで解決・アップロードします。一方、スタンドアロンビルドでは起動時に登録済みのモデルとテクスチャを VRAM へプリロード（`PreloadGameAssets`）し、プレイ中のヒッチを抑制します。

#### シーンのシリアライズとデシリアライズ
`SceneSerializer` は RapidJSON を使ってシーンを可読な JSON 形式で保存・読み込みします:
- **シリアライズ**: 全エンティティのコンポーネントをアセット UUID 参照として書き出し、一時的な `entt::entity` の親子関係ハンドルを安定した `IDComponent` の 64 ビット ID に変換して保存します。
- **4 パス・デシリアライズ**:
  1. **検証パス（Validation Pass）**: 参照されるすべてのメッシュ、PBR マップ、物理メッシュ、オーディオ、フォント、スクリプトがレジストリ（エディタモードではディスク上）に存在するか事前検証します。欠損があれば現在のシーンを変更する前にエラーモーダルを出して安全に中断します。
  2. **シーンクリア**: 現在のエンティティ、Jolt ボディ、オーディオハンドルを安全に解放します。
  3. **エンティティ再構築**: `savedID -> entt::entity` の変換マップを構築しながらエンティティとコンポーネントを復元します。
  4. **階層復元**: 変換マップを用いて `RelationshipComponent` の親・子・兄弟リンクを再接続します。

#### アセットパッキングと仮想ファイルシステム（VFS）
- **`PAK2` バイナリアーカイブ（`AssetManager::PackAssets`）**: レジストリに登録されたすべての物理ファイルを単一の `data.pak` アーカイブにまとめます:
  - 4 バイトのマジックヘッダー（`"PAK2"`） + `uint32_t` アセット数
  - 各エントリの `UUID`、`AssetType`、ファイルパス文字列、バイトオフセット、バイトサイズを格納する動的 **Table of Contents (TOC)**
  - 連結された生のバイナリペイロード
- **透過的な VFS マウント（`MountVFS` / `ReadAssetFromVFS`）**: スタンドアロンビルドでは `DX11Runtime` が起動時に `data.pak` をマウントし、アセットレジストリと TOC オフセットテーブルをメモリ上に再構築します。すべてのローダーが個別ファイルに触れることなく VFS メモリバッファから透過的に読み込みます:
  - **モデル**: カスタム `Assimp::IOSystem`（`VFSIOSystem`）および `Assimp::IOStream`（`VFSIOStream`）
  - **テクスチャ**: `stbi_load_from_memory`
  - **シェーダー**: 事前コンパイル済み `.cso` バイトコード
  - **オーディオ**: `miniaudio` メモリデコーダ（`ma_decoder_init_memory`）
  - **フォント**: メモリ上の `.ttf` バッファを `stbtt_BakeFontBitmap` でベイク
  - **スクリプト & シーン**: 文字列として抽出し、`sol::state::safe_script` および RapidJSON へ直接入力

#### スタンドアロンゲームのエクスポートパイプライン
エディタのツールバーから **Export Game** をクリックすると `AssetManager::ExportProject()` が実行され、配布可能なゲームディレクトリ（`ExportedGame/`）を自動生成します:
1. アクティブシーンをシリアライズし、新規アタッチされたスクリプトをレジストリに登録します。
2. プロジェクトの全アセット、コンパイル済み `.cso` シェーダー、スクリプト、シーンを `ExportedGame/data.pak` にパックします。
3. 事前ビルド済みの `DX11Runtime.exe` と必要な DLL を `RuntimeTemplate/` からコピーします。
4. アクティブシーンを起動レベルとして指定した `ExportedGame/enginefiles/Launch.txt` を出力します。
5. スタンドアロン実行可能な `ExportedGame/` フォルダを開きます。

---

### オーディオと入力

- **オーディオ（`AudioManager` & `AudioSystem`）**: `miniaudio` をラップし、ループ再生・音量制御・生成時再生に対応した 2D および 3D 空間オーディオエミッタ（`AudioComponent`）を提供します。Play モード中は毎フレーム `AudioSystem` がアクティブカメラの位置・前方ベクトルをリスナーに同期し、エンティティのワールド行列から分解した位置を 3D エミッタへ反映します。
- **ウィンドウと入力（`SDL2` & `InputManager`）**: SDL2 が Windows の `HWND`（`SDL_SysWMinfo` 経由で Direct3D 11 へ受け渡し）、高精度フレームタイマー、ウィンドウリサイズイベント、入力ポーリングを管理します。`InputManager` は現在フレームと前フレームのキー状態（`IsKeyDown`, `IsKeyPressed`, `IsKeyReleased`）、マウス移動量、ホイールスクロール量、相対マウスキャプチャを追跡します。

---

### CMake 構成

トップレベルの [CMakeLists.txt](CMakeLists.txt) は **CMake 3.20+**、**C++17**、MSVC の警告／準拠フラグ（`/permissive- /W4 /EHsc` および外部ヘッダー向け `/external:W0`）、共有プリコンパイル済みヘッダーを構成します。

- **デュアル実行可能ターゲット**:
  - **`DX11Editor`** *（Visual Studio のデフォルトスタートアッププロジェクト）* — Dear ImGui、ImGuizmo、ディスクベースのアセット編集機能を備えたフルエディタ版。
  - **`DX11Runtime`** — `DIST_BUILD` マクロ付きでコンパイルされ、エディタ UI を除外して `data.pak` VFS + Play モードへ直接起動するスタンドアロン版。
- **依存関係**: vcpkg manifest モードで `SDL2`、`assimp`、`EnTT`、`RapidJSON`、`imgui`、`Jolt`、`lua`、`sol2`、`stb`、`miniaudio` を解決し、Windows の DirectX ライブラリ（`d3d11`, `dxgi`, `d3dcompiler`, `dxguid`）をリンクします。
- **アセットコピーとランタイムテンプレートのパッケージング**:
  - カスタムビルドターゲット（`CopyShaders`, `CopyAssets`, `CopyEngineFiles`）が `shaders/`、`assets/`、`enginefiles/` を実行出力ディレクトリへコピーします。
  - `DX11Runtime` の `POST_BUILD` ステップにより、`DX11Runtime.exe` と必要なすべての `.dll` を `bin/RuntimeTemplate/` へ自動配置するため、エディタの **Export Game** 機能から即座にスタンドアロンゲームを出力できます。

---

## プロジェクトのセットアップ（Visual Studio）

本プロジェクトは **CMake + vcpkg（manifest モード）** を使用しているため、CMake の構成時にすべてのサードパーティ依存関係が自動でダウンロード・ビルドされます。

### 前提条件

- **Windows 10 / 11**（DirectX 11 ランタイム）
- **Visual Studio 2022**
  - **ワークロード**: C++ によるデスクトップ開発
  - **コンポーネント**: MSVC、Windows 用 C++ CMake ツール、Git

### 1. リポジトリをクローン

```bash
git clone https://github.com/NathanAung/DX11GameEngine.git
```

### 2. CMake と vcpkg のインストール（自動セットアップ）

**CMake** および **vcpkg** がまだインストールされていない場合は、`tools` フォルダ内のセットアップスクリプトを実行してください:

```bat
tools\SetupProject.bat
```

このスクリプトは以下を自動で実行します:
- **CMake** をダウンロードし、ローカル AppData フォルダ（`%LOCALAPPDATA%\w64cmake`）にインストールします。
- **vcpkg** をシステムドライブ直下（例: `C:\vcpkg`）にクローンしてブートストラップします。

続行する前に `%SystemDrive%\vcpkg\scripts\buildsystems\vcpkg.cmake` が存在することを確認してください。

> プロジェクトセットアップツール提供: Francesco Paolo Mariani

### 3. Visual Studio でプロジェクトを開く

1. **Visual Studio 2022** を起動します。
2. **ファイル → 開く → フォルダー...** を選択し、クローンした `DX11GameEngine` フォルダを開きます。
3. Visual Studio が `CMakeLists.txt` と `CMakeSettings.json` を自動検出します。

### 4. CMake の設定（Visual Studio UI）

1. **プロジェクト → DX11GameEngine の CMake 設定** を開きます。
2. 構成（例: `x64-Debug` または `x64-Release`）を選択します。
3. **CMake ツールチェーンファイル**（`CMAKE_TOOLCHAIN_FILE`）が以下に設定されていることを確認します:
   - `${env.SystemDrive}/vcpkg/scripts/buildsystems/vcpkg.cmake`
4. 設定を保存します。Visual Studio が CMake を構成し、vcpkg を呼び出して `vcpkg.json` に記載された依存関係をインストールします。
   - *備考: 初回の構成時は依存関係のビルドに数分かかる場合があります。*

### 5. ビルドと実行

1. Visual Studio のツールバーで以下を選択します:
   - **構成**: `x64-Debug` または `x64-Release`
   - **スタートアップ項目**: `DX11Editor.exe`
2. **ビルド**: **ビルド → すべてビルド** を選択します（`DX11Editor` と `DX11Runtime` の両方をビルドすることで、エディタの **Export Game** 機能に必要な `RuntimeTemplate/` も生成されます）。
3. **実行**: **デバッグ → デバッグなしで開始（`Ctrl + F5`）** を選択します。

---

## アーカイブ

> **注意:** 以下のポートフォリオ、動画、スクリーンショットは旧バージョンの Step Engine のものであり、最新機能（ImGui エディタ、カスケードシャドウマップ、法線マッピング、Lua スクリプティング、オーディオ、2D UI、VFS、ゲームエクスポート等）は反映されていません。

### ポートフォリオ（旧版）
初期の設計や実装内容については、アーカイブされたポートフォリオをご覧ください:
- [ポートフォリオを見る（Google Drive）](https://drive.google.com/file/d/1DbQy8O9z-OBZ13gIXDv29bzn-9GjP03G/view?usp=sharing)

### 開発アップデート動画（2025/12/22）
[![Development Update](https://i.imgur.com/VwbFdgl.jpeg)](https://youtu.be/ANGTMzYf98M)

### 初期デモ動画（クリックで再生）

#### ライティングデモ
[![Lighting Demo](https://i.imgur.com/0RIBnX6.jpeg)](https://youtu.be/dPvAgLWRbKk)

#### ガルトンボード物理デモ
[![Galton Board Physics Demo](https://i.imgur.com/Bj7wcyO.jpeg)](https://youtu.be/JmqsDxMvVeI)

#### Wall Smasher ゲームデモ
[![Wall Smasher Game Demo](https://i.imgur.com/jCIpugV.jpeg)](https://youtu.be/DlM2aOIT1i8)

#### 3D モデルデモ
[![3D Model Demo](https://i.imgur.com/4AC6h1Z.jpeg)](https://youtu.be/XPVFZV_9ea4)

### 初期ギャラリー

#### PBR ライティング
- メタリック／ラフネスによる PBR マテリアル
<img src="https://i.imgur.com/FqLpL3O.jpeg"/>

- ディレクショナル、スポット、ポイントライト
<img src="https://i.imgur.com/fT8hAZw.jpeg"/>

#### 3D モデルの読み込みとレンダリング
<img src="https://i.imgur.com/qVgbAAt.jpeg"/>

<img src="https://i.imgur.com/nliMOQQ.jpeg"/>
