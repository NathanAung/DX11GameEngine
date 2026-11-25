# Lua Scripting Bindings

Gameplay scripts are powered by **Lua** and **sol2**. Every binding documented below is verified directly against `Scene::InitializeLuaBindings` (`src/Engine/Scene.cpp`) and `Engine::ScriptEntity` (`include/Engine/ScriptEntity.h`).

---

## Execution Environment & Standard Libraries

- **Standard Libraries Loaded**: `base`, `math`, `table`, `string` (`m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string)`).
- **Per-Script Sandboxing**: Every `ScriptInstance` runs inside its own `sol::environment` inheriting from `m_lua.globals()`, with a pre-populated `self` variable of usertype `Entity` (`Engine::ScriptEntity`) referring to the owning entity.

---

## Lifecycle & Physics Callbacks

Define any subset of these global functions inside a `.lua` script; `ScriptSystem` and `PhysicsContactListener` cache and invoke them automatically:

| Callback Signature | When Invoked |
| :--- | :--- |
| `function OnCreate()` | Once when Play Mode starts, or immediately prior to the first `OnUpdate` if attached dynamically via `AddLuaScript`. |
| `function OnUpdate(dt)` | Every frame during Play Mode. `dt` (`number`) is the frame delta time in seconds. |
| `function OnDestroy()` | When the entity is destroyed (`Scene::DestroyEntity`) or when Play Mode stops / restarts. |
| `function OnCollisionEnter(other)` | Flushed on the main thread after `PhysicsManager::Update` when a new Jolt contact point is added. `other` is an `Entity` (`ScriptEntity`). |
| `function OnCollisionExit(other)` | Flushed on the main thread when a Jolt contact pair separates. `other` is an `Entity` (`ScriptEntity`). |

---

## Global Tables & Singletons

> **Syntax Convention:** `Input` is a bound C++ `InputManager*` usertype instance—call its methods with **colon** syntax (`Input:IsKeyPressed(Key.Space)`). `Physics`, `Scene`, `Time`, and `Game` are plain Lua tables containing closures/fields—access them with **dot** syntax (`Physics.CastRay(...)`, `Scene.FindEntityByName(...)`, `Game.Restart()`).

### `Input` (`Engine::InputManager` usertype)

| Lua Call | Return Type | Description |
| :--- | :--- | :--- |
| `Input:IsKeyDown(key)` | `boolean` | `true` every frame while `key` (`Key` enum) is held down. |
| `Input:IsKeyPressed(key)` | `boolean` | `true` *only* on the exact frame `key` transitions from up to down. |
| `Input:IsKeyReleased(key)` | `boolean` | `true` *only* on the exact frame `key` transitions from down to up. |

### `Physics` Table

| Lua Call | Return Type | Description |
| :--- | :--- | :--- |
| `Physics.CastRay(ox, oy, oz, dx, dy, dz)` | `Entity \| nil` | Fires a world-space ray from `(ox, oy, oz)` along `(dx, dy, dz)` via `Scene::CastRay` (Jolt narrow-phase up to 1000 units, with OBB fallback). Returns the hit `Entity`, or `nil` if nothing was hit. |

### `Scene` Table

| Lua Call | Return Type | Description |
| :--- | :--- | :--- |
| `Scene.FindEntityByName(name)` | `Entity \| nil` | Searches `NameComponent` entries for an exact match with `name` (`string`). Returns `Entity` or `nil`. |

### `Time` & `Game` Tables

| Lua Expression / Call | Type | Description |
| :--- | :--- | :--- |
| `Time.deltaTime` | `number` | Current frame delta time in seconds, updated at the start of `ScriptSystemUpdate`. |
| `Game.Restart()` | `void` | Sets `Scene::m_requestRestart = true`, restoring the clean pre-play snapshot and rebooting all scripts at frame end. |
| `Game.Quit()` | `void` | Sets `Scene::m_requestQuit = true`, cleanly shutting down the application at frame end. |

---

## Usertypes

### `Vector3` (`DirectX::XMFLOAT3`)

- **Constructors**: `Vector3.new()` or `Vector3.new(x, y, z)`
- **Properties**:
    - `v.x` (`number`)
    - `v.y` (`number`)
    - `v.z` (`number`)

### `TransformComponent`

Retrieved via `self:GetTransform()`.

| Member | Kind | Signature / Type | Description |
| :--- | :--- | :--- | :--- |
| `position` | Property | `Vector3` | Gets a copy of local position, or assigns a `Vector3` and sets `isDirty = true`. |
| `scale` | Property | `Vector3` | Gets a copy of local scale, or assigns a `Vector3` and sets `isDirty = true`. |
| `SetPosition` | Method | `tc:SetPosition(x, y, z)` | Sets `tc.position = {x, y, z}` and marks `tc.isDirty = true`. |
| `SetScale` | Method | `tc:SetScale(x, y, z)` | Sets `tc.scale = {x, y, z}` and marks `tc.isDirty = true`. |

> **Important Value-Semantics Note:** Reading `tc.position` or `tc.scale` returns a `Vector3` **by value**. Mutating a single field directly (`self:GetTransform().position.x = 5`) modifies a temporary copy and will *not* move the entity. Always use `self:GetTransform():SetPosition(x, y, z)` or assign a full `Vector3` (`tc.position = Vector3.new(x, y, z)`).

---

### `Entity` (`Engine::ScriptEntity`)

Bound in Lua as `Entity` and injected into every script environment as `self`. Use colon syntax (`self:Method(...)` or `other:Method(...)`).

#### Identity, Activation, Hierarchy & Transforms

| Method | Return | Description |
| :--- | :--- | :--- |
| `entity:GetName()` | `string` | Returns `NameComponent::name`, or `"Unknown"` if absent. |
| `entity:IsActive()` | `boolean` | Returns `NameComponent::isActive` (`false` if invalid). |
| `entity:SetActive(active)` | `void` | Sets `NameComponent::isActive` (`boolean`). |
| `entity:GetTransform()` | `TransformComponent&` | Returns a reference to the entity's `TransformComponent`. |
| `entity:GetForwardVector()` | `Vector3` | Returns `(0, 0, 1)` rotated by the entity's normalized quaternion. |
| `entity:GetRightVector()` | `Vector3` | Returns `(1, 0, 0)` rotated by the entity's normalized quaternion. |
| `entity:SetParent(parentEntity)` | `void` | Parents this entity under `parentEntity` (`Entity`) via `Scene::ParentEntity`, preserving world transform. |

#### Spawning & Destruction

| Method | Return | Description |
| :--- | :--- | :--- |
| `entity:Instantiate(name)` | `Entity` | Spawns an empty entity (`ID`, `Name`, `Transform`) named `name`. |
| `entity:InstantiateCube(name)` | `Entity` | Spawns an entity with a unit cube `MeshRendererComponent`. |
| `entity:InstantiateSphere(name)` | `Entity` | Spawns an entity with a unit sphere `MeshRendererComponent`. |
| `entity:Destroy()` / `entity:Destroy(delay)` | `void` | Queues this entity in `Scene::SubmitForDestruction` with optional `delay` in seconds (default `0.0`). |

#### Component Checks & Runtime Attachment

| Method | Return | Description |
| :--- | :--- | :--- |
| `entity:HasMesh()` | `boolean` | `true` if `MeshRendererComponent` is attached. |
| `entity:HasRigidBody()` | `boolean` | `true` if `RigidBodyComponent` is attached. |
| `entity:HasLight()` | `boolean` | `true` if `LightComponent` is attached. |
| `entity:HasAudio()` | `boolean` | `true` if `AudioComponent` is attached. |
| `entity:HasScript()` | `boolean` | `true` if `LuaScriptComponent` is attached. |
| `entity:AddMeshRenderer(meshType)` | `void` | Attaches/updates `MeshRendererComponent` (`MeshType.Cube`, `Sphere`, `Capsule`, or `Custom`) with `MaterialType::LitColor`. |
| `entity:AddRigidBody(shape, motion, size, mass, restitution)` | `void` | Attaches/rebuilds `RigidBodyComponent`. Maps `size` to `radius` (`Sphere`), `{size, size, size}` (`Box` half-extents or `Mesh` `colliderScale`), or `radius = size, height = size * 2` (`Capsule`). |
| `entity:AddLuaScript(filepath)` | `void` | Appends a new `ScriptInstance` (`filepath`) to `LuaScriptComponent`; initialized on the next `ScriptSystemUpdate`. |
| `entity:AddAudioComponent(filepath, is3D, loop, playOnCreate)` | `void` | Attaches/updates `AudioComponent`, importing `filepath` as `AssetType::Audio`. |
| `entity:AddSpriteRenderer()` | `void` | Attaches `SpriteRendererComponent` if not already present. |
| `entity:AddTextRenderer()` | `void` | Attaches `TextRendererComponent` if not already present. |

#### Physics Forces & Velocity

| Method | Return | Description |
| :--- | :--- | :--- |
| `entity:ApplyLinearImpulse(x, y, z)` | `void` | Applies an instantaneous world-space impulse (`JPH::BodyInterface::AddImpulse`) to a valid rigid body. |
| `entity:SetLinearVelocity(x, y, z)` | `void` | Directly sets linear velocity (`JPH::BodyInterface::SetLinearVelocity`). |
| `entity:GetLinearVelocity()` | `Vector3` | Returns current linear velocity from Jolt (`{0, 0, 0}` if invalid). |

#### Materials, Lighting, Audio & 2D UI Mutation

| Method | Return | Description |
| :--- | :--- | :--- |
| `entity:SetMesh(filepath)` | `void` | Imports `filepath` (e.g., `"assets/Models/foo.obj?mesh=0"`) and assigns `MeshRendererComponent::meshID`. |
| `entity:SetAlbedo(filepath)` | `void` | Imports texture `filepath`, assigns `albedoMapID`, and switches `matType` to `Textured`. |
| `entity:SetNormal(filepath)` | `void` | Imports texture `filepath`, assigns `normalMapID`, and switches `matType` to `Textured`. |
| `entity:SetORM(filepath)` | `void` | Imports texture `filepath`, assigns `ormMapID`, and switches `matType` to `Textured`. |
| `entity:SetLightColor(r, g, b)` | `void` | Updates `LightComponent::color` (`float` RGB). |
| `entity:SetLightIntensity(intensity)` | `void` | Updates `LightComponent::intensity`. |
| `entity:PlayAudio()` | `void` | Plays (or restarts) the entity's `AudioComponent`, lazily loading the sound handle if not yet loaded. |
| `entity:StopAudio()` | `void` | Stops active playback on the entity's `AudioComponent`. |
| `entity:SetAudioVolume(volume)` | `void` | Clamps `volume` to `[0.0, 1.0]` and updates `AudioComponent::volume`. |
| `entity:SetSprite(filepath)` | `void` | Imports texture `filepath` and assigns `SpriteRendererComponent::textureID`. |
| `entity:SetSpriteTint(r, g, b, a)` | `void` | Updates `SpriteRendererComponent::tintColor`. |
| `entity:SetText(text)` | `void` | Updates `TextRendererComponent::text`. |
| `entity:SetTextColor(r, g, b, a)` | `void` | Updates `TextRendererComponent::color`. |
| `entity:SetTextPosition(x, y)` | `void` | Updates `TextRendererComponent::position` in screen pixels. |
| `entity:SetFont(filepath, size)` | `void` | Sets `fontSize = size` and registers `<filepath>?size=<int(size)>` for JIT font atlas baking. |

---

## Bound Enumerations

| Lua Enum | Values |
| :--- | :--- |
| **`MeshType`** | `MeshType.Cube`, `MeshType.Sphere`, `MeshType.Capsule`, `MeshType.Custom` |
| **`RBShape`** | `RBShape.Box`, `RBShape.Sphere`, `RBShape.Capsule`, `RBShape.Mesh` |
| **`RBMotion`** | `RBMotion.Static`, `RBMotion.Dynamic` |
| **`Key`** | Letters: `Key.A` .. `Key.Z`<br/>Digits: `Key.Num0` .. `Key.Num9`<br/>Arrows: `Key.Up`, `Key.Down`, `Key.Left`, `Key.Right`<br/>Actions: `Key.Space`, `Key.Escape`, `Key.Enter`, `Key.Tab`, `Key.Backspace`<br/>Modifiers: `Key.LShift`, `Key.RShift`, `Key.LControl`, `Key.RControl`, `Key.LAlt`, `Key.RAlt` |

> Note: Pressing `Escape` is also handled globally in `src/main.cpp` (`SDL_SCANCODE_ESCAPE`) to close the window.
