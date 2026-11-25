# Lua ゲームプレイスクリプトの作成

Step Engine は **sol2** を介して **Lua** を組み込んでおり、C++ を再コンパイルすることなくゲームプレイロジック、物理インタラクション、エンティティ生成、動的な HUD 更新を記述できます。

バインドされているすべての型や列挙型の完全なシグネチャ一覧については、[Lua スクリプティングバインディング](../api/lua-scripting.md) を参照してください。

---

## ステップ 1: スクリプトの作成とアタッチ

1. `assets/Scripts/` 内に新しい `.lua` ファイルを作成します（例: `assets/Scripts/PlayerController.lua`）。
   *(`DX11Editor` を `bin/` から実行中の場合は、`bin/assets/Scripts/` にもコピーするか、CMake ビルドを実行して `CopyAssets` で同期してください。)*
2. **Hierarchy** パネルで対象のエンティティを選択します。
3. **Inspector** で **Add Component -> Lua Script** をクリックします。
4. **Add Script Slot** をクリックし、以下のいずれかの方法で割り当てます：
    - ドロップダウンリストから `PlayerController.lua` を選択する
    - **Content Browser** から `PlayerController.lua` をスクリプトスロットへ直接ドラッグ＆ドロップする（または **Add Script Slot** ボタン上に直接ドロップしてスロット追加と割り当てを同時に行う）

1つのエンティティには **複数のスクリプト** をアタッチできます。各スロットは所有エンティティ `Entity` が `self` としてバインドされた独立した `sol::environment` サンドボックス内で実行されます。

---

## ステップ 2: ライフサイクルおよび衝突コールバックの実装

各スクリプトでは、最大5つのエンジンコールバック関数を定義できます。

```lua
-- Local variables stay private to this specific ScriptInstance!
local elapsed = 0.0

function OnCreate()
    print(self:GetName() .. " spawned!")
end

function OnUpdate(dt)
    elapsed = elapsed + dt
end

function OnCollisionEnter(other)
    print(self:GetName() .. " hit " .. other:GetName())
end

function OnCollisionExit(other)
    print(self:GetName() .. " separated from " .. other:GetName())
end

function OnDestroy()
    print(self:GetName() .. " cleaned up after " .. tostring(elapsed) .. "s")
end
```

---

## 実践レシピ集

### レシピ 1: 物理ベースのプレイヤー移動とジャンプ

エンティティに Inspector（または `OnCreate` 内の `AddRigidBody`）で **Dynamic** な `Rigidbody` コンポーネントを付与し、`Input` に応じて撃力（インパルス）または速度を与えます。

```lua
local moveForce = 0.25
local jumpImpulse = 6.0

function OnUpdate(dt)
    local dx = 0.0
    local dz = 0.0

    if Input:IsKeyDown(Key.W) then dz = dz + 1.0 end
    if Input:IsKeyDown(Key.S) then dz = dz - 1.0 end
    if Input:IsKeyDown(Key.A) then dx = dx - 1.0 end
    if Input:IsKeyDown(Key.D) then dx = dx + 1.0 end

    if dx ~= 0.0 or dz ~= 0.0 then
        self:ApplyLinearImpulse(dx * moveForce, 0.0, dz * moveForce)
    end

    if Input:IsKeyPressed(Key.Space) then
        self:ApplyLinearImpulse(0.0, jumpImpulse, 0.0)
    end
end
```

### レシピ 2: 実行時のスポーン・オーディオ再生・時限破棄

新しいエンティティを生成してメッシュ・コライダー・オーディオ・スクリプトを動的に付与し、`Destroy(delay)` で一定時間後の自動破棄を予約できます。

```lua
-- Spawner.lua
function OnUpdate(dt)
    if Input:IsKeyPressed(Key.Space) then
        local ball = self:Instantiate("Bouncing Ball")
        local pos = self:GetTransform().position
        ball:GetTransform():SetPosition(pos.x, pos.y + 2.0, pos.z)

        ball:AddMeshRenderer(MeshType.Sphere)
        -- RBShape.Sphere, Dynamic motion, radius=0.5, mass=1.0, restitution=0.8
        ball:AddRigidBody(RBShape.Sphere, RBMotion.Dynamic, 0.5, 1.0, 0.8)
        ball:AddAudioComponent("assets/Audio/spawn.mp3", true, false, true)

        -- Automatically destroy the spawned ball after 5 seconds
        ball:Destroy(5.0)
    end
end
```

### レシピ 3: 前方へのレイキャスト

`self:GetForwardVector()` と `Physics.CastRay` を組み合わせて、エンティティ前方のオブジェクトを検出します。自身のコライダーへの誤ヒットを防ぐため、発射原点を少し前方にオフセットさせます。

```lua
function OnUpdate(dt)
    if Input:IsKeyPressed(Key.F) then
        local pos = self:GetTransform().position
        local fwd = self:GetForwardVector()

        local ox = pos.x + fwd.x * 0.6
        local oy = pos.y + fwd.y * 0.6
        local oz = pos.z + fwd.z * 0.6

        local hit = Physics.CastRay(ox, oy, oz, fwd.x, fwd.y, fwd.z)
        if hit ~= nil then
            print("Hit entity: " .. hit:GetName())
        end
    end
end
```

### レシピ 4: シーン内検索と親子付けの変更

`Scene.FindEntityByName` で別のエンティティを名前検索し、ワールド変換を維持したまま自身の子としてアタッチします。

```lua
function OnCreate()
    local weapon = Scene.FindEntityByName("LaserRifle")
    if weapon ~= nil then
        weapon:SetParent(self)
    end
end
```

### レシピ 5: 動的 2D HUD 更新とゲーム再起動・終了

Lua から直接スクリーン空間のテキストやスプライトを設定したり、シーンの再起動やアプリ終了を呼び出したりできます。

```lua
local score = 0

function OnCreate()
    self:AddTextRenderer()
    self:SetFont("assets/Fonts/FiraCode-VariableFont_wght.ttf", 32)
    self:SetTextPosition(24.0, 24.0)
    self:SetTextColor(1.0, 1.0, 1.0, 1.0)
    self:SetText("Score: 0")
end

function OnUpdate(dt)
    if Input:IsKeyPressed(Key.E) then
        score = score + 10
        self:SetText("Score: " .. tostring(score))
    end

    if Input:IsKeyPressed(Key.R) then
        Game.Restart()
    elseif Input:IsKeyPressed(Key.Q) then
        Game.Quit()
    end
end
```

---

## よくある落とし穴とベストプラクティス

1. **コロン (`:`) とドット (`.`) の使い分け**:
    - ユーザー型インスタンス（`self:...`、`other:...`、`tc:SetPosition(...)`、および `Input:IsKeyDown(...)`）には **コロン (`:`)** 構文を使用します。
    - グローバル名前空間テーブル（`Physics.CastRay(...)`、`Scene.FindEntityByName(...)`、`Time.deltaTime`、`Game.Restart()`、`Game.Quit()`）には **ドット (`.`)** 構文を使用します。
2. **`TransformComponent` の座標・スケール更新**:
    - `self:GetTransform().position` は `Vector3` を **値渡し（コピー）** で返します。`self:GetTransform().position.x = 10` と記述しても一時コピーが書き換わるだけで `isDirty = true` が設定されません。
    - 必ず `self:GetTransform():SetPosition(x, y, z)` を呼び出すか、`Vector3` を丸ごと代入（`self:GetTransform().position = Vector3.new(x, y, z)`）してください。
3. **動的リジッドボディの移動方法**:
    - Play モード中、有効な `RigidBodyComponent` を持つエンティティの `TransformComponent` 座標は毎フレーム `PhysicsSystem` によって Jolt のシミュレーション結果で上書きされます。物理オブジェクトを動かす際は `SetPosition` ではなく、`ApplyLinearImpulse` または `SetLinearVelocity` を使用してください。
4. **スタンドアロン VFS ビルドとの互換性**:
    - スタンドアロンの `DX11Runtime` ビルドにおいて、`AddLuaScript("assets/Scripts/Foo.lua")` や `AddAudioComponent` / `SetAlbedo` / `SetFont` 等で実行時に動的ロードされるアセットは、**Export Game** をクリックする前の段階で `enginefiles/AssetRegistry.json` に登録されている必要があります（未登録のファイルは `data.pak` に含まれません）。エクスポート前に一度 `DX11Editor` 上でシーンを Play して動作確認し、`Ctrl + S` で保存しておくことで動的アセットも確実に登録されます。
