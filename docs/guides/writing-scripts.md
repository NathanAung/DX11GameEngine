# Writing Lua Gameplay Scripts

Step Engine embeds **Lua** via **sol2** so you can write gameplay logic, physics interactions, entity spawners, and dynamic HUD updates without recompiling C++.

For the complete signature list of every bound type and enum, see [Lua Scripting Bindings](../api/lua-scripting.md).

---

## Step 1: Create & Attach a Script

1. Create a new `.lua` file inside `assets/Scripts/` (for example, `assets/Scripts/PlayerController.lua`).
   *(If `DX11Editor` is running from `bin/`, ensure the file is also copied into `bin/assets/Scripts/` or run a quick CMake build so `CopyAssets` syncs it.)*
2. Select the target entity in the **Hierarchy** panel.
3. In the **Inspector**, click **Add Component -> Lua Script**.
4. Click **Add Script Slot**, then either:
    - Choose `PlayerController.lua` from the dropdown list, **or**
    - Drag `PlayerController.lua` directly from the **Content Browser** onto the script slot (or onto the **Add Script Slot** button to append a new slot in one step).

You can attach **multiple scripts** to the same entity. Each slot executes inside its own sandboxed `sol::environment` with `self` bound to the owning `Entity`.

---

## Step 2: Implement Lifecycle & Collision Callbacks

Every script can define up to five engine callbacks:

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

## Common Gameplay Recipes

### Recipe 1: Physics-Driven Player Movement & Jumping

Ensure the entity has a **Dynamic** `Rigidbody` component attached in the Inspector (or via `AddRigidBody` in `OnCreate`), then apply impulses or velocities based on `Input`:

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

### Recipe 2: Runtime Spawning, Audio & Timed Destruction

You can spawn new entities, attach meshes/colliders/audio/scripts dynamically, and schedule automatic cleanup using `Destroy(delay)`:

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

### Recipe 3: Forward Raycasting

Use `self:GetForwardVector()` and `Physics.CastRay` to detect objects in front of an entity. Offset the ray origin slightly outside the caster's own collider to avoid self-hits:

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

### Recipe 4: Scene Queries & Hierarchy Reparenting

Locate another entity by name with `Scene.FindEntityByName` and attach it as a child while preserving its world transform:

```lua
function OnCreate()
    local weapon = Scene.FindEntityByName("LaserRifle")
    if weapon ~= nil then
        weapon:SetParent(self)
    end
end
```

### Recipe 5: Dynamic 2D HUD & Game Restart / Quit

Configure screen-space text and sprites or trigger level restarts directly from Lua:

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

## Pitfalls & Best Practices

1. **Colon (`:`) vs. Dot (`.`) Call Syntax**:
    - Use **colon** syntax on usertype instances (`self:...`, `other:...`, `tc:SetPosition(...)`, and `Input:IsKeyDown(...)`).
    - Use **dot** syntax on global namespace tables (`Physics.CastRay(...)`, `Scene.FindEntityByName(...)`, `Time.deltaTime`, `Game.Restart()`, `Game.Quit()`).
2. **Updating `TransformComponent` Position & Scale**:
    - `self:GetTransform().position` returns a `Vector3` **by value**. Do **not** write `self:GetTransform().position.x = 10`—that modifies a temporary copy without setting `isDirty = true`.
    - Instead, call `self:GetTransform():SetPosition(x, y, z)` or assign a `Vector3` (`self:GetTransform().position = Vector3.new(x, y, z)`).
3. **Moving Dynamic Rigid Bodies**:
    - During Play Mode, `PhysicsSystem` overwrites `TransformComponent` positions of active `RigidBodyComponent` entities from Jolt every frame. Move physics-enabled entities using `ApplyLinearImpulse` or `SetLinearVelocity` rather than `SetPosition`.
4. **Standalone VFS Compatibility**:
    - Any script attached via `AddLuaScript("assets/Scripts/Foo.lua")` or asset loaded dynamically via `AddAudioComponent` / `SetAlbedo` / `SetFont` in a standalone `DX11Runtime` build must already be registered in `enginefiles/AssetRegistry.json` prior to clicking **Export Game** so that `PackAssets` includes it inside `data.pak`. Playtesting your scene once in `DX11Editor` and pressing `Ctrl + S` before exporting guarantees that dynamic assets are registered!
