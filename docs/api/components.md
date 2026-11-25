# ECS Components & Enums

All ECS components and associated enumerations are declared in `include/Engine/Components.h` inside `namespace Engine`.

---

## Enumerations

### `Engine::MeshType`
```cpp
enum class MeshType { Cube, Sphere, Capsule, Custom };
```
Selects a built-in primitive mesh or custom imported mesh when attaching a `MeshRendererComponent` (also exposed to Lua as `MeshType`).

### `Engine::MaterialType`
```cpp
enum class MaterialType { LitColor, UnlitColor, Textured };
```
- `LitColor` (`0`) — Shaded with Cook-Torrance PBR (`BasicPS.hlsl`) using `baseColor`, `roughness`, `metallic`, and 1x1 default textures; casts shadows.
- `UnlitColor` (`1`) — Flat solid color (`UnlitPS.hlsl`) using `baseColor`; skipped during the shadow pass.
- `Textured` (`2`) — Shaded with Cook-Torrance PBR (`BasicPS.hlsl`) sampling `albedoMapID` (`t0`), `normalMapID` (`t1`), and `ormMapID` (`t2`), modulated by `baseColor`, `roughness`, and `metallic`; casts shadows.

### `Engine::CameraControlMode`
```cpp
enum class CameraControlMode { EditorCam = 0, Scripted };
```
Determines whether `EditorCameraInputSystem` processes fly-camera keyboard/mouse input for an entity (`EditorCam`) or leaves camera motion to scripts (`Scripted`).

### `Engine::LightType`
```cpp
enum class LightType : unsigned int
{
    Directional = 0,
    Point       = 1,
    Spot        = 2
};
```
Matches the HLSL `LightData::type` integer values in `BasicPS.hlsl`.

### `Engine::RBShape` & `Engine::RBMotion`
```cpp
enum class RBShape  { Box, Sphere, Capsule, Mesh };
enum class RBMotion { Static, Dynamic };
```
Configure collider geometry and Jolt motion/layer behavior (`Layers::NON_MOVING` vs. `Layers::MOVING`) on `RigidBodyComponent` (also exposed to Lua as `RBShape` and `RBMotion`).

---

## Identity, Hierarchy & Transform Components

### `IDComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `id` | `uint64_t` | `0` | Stable 64-bit entity identifier persisted as `"EntityID"` in JSON scene files. |

### `NameComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `name` | `std::string` | `""` | Human-readable display name shown in the Hierarchy panel and searchable via `Scene::FindEntityByName`. |
| `isActive` | `bool` | `true` | Master entity activation toggle. When `false`, rendering, physics bodies, raycasts, and audio emitters are disabled. |

### `RelationshipComponent`
Intrusive doubly-linked hierarchy node. Managed via `Scene::ParentEntity` and `Scene::UnparentEntity`.

| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `parent` | `entt::entity` | `entt::null` | Parent entity handle (`entt::null` for root entities). |
| `firstChild` | `entt::entity` | `entt::null` | First child in this entity's child list. |
| `prevSibling` | `entt::entity` | `entt::null` | Previous sibling sharing the same parent. |
| `nextSibling` | `entt::entity` | `entt::null` | Next sibling sharing the same parent. |

### `TransformComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `position` | `DirectX::XMFLOAT3` | `{0.0f, 0.0f, 0.0f}` | Local-space translation relative to `parent` (or world space if root). |
| `rotation` | `DirectX::XMFLOAT4` | `{0.0f, 0.0f, 0.0f, 1.0f}` | Local-space orientation stored as a normalized `(x, y, z, w)` quaternion. |
| `scale` | `DirectX::XMFLOAT3` | `{1.0f, 1.0f, 1.0f}` | Local-space scale factors. |
| `worldMatrix` | `DirectX::XMFLOAT4X4` | Identity `4x4` | Cached row-major world transformation matrix updated each frame by `TransformSystem`. |
| `isDirty` | `bool` | `true` | Marks the transform as modified so `TransformSystem` cascades dirty state to descendants. |

---

## Rendering, Camera & Lighting Components

### `MeshRendererComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | Enables or disables mesh rendering and shadow casting for this component. |
| `meshID` | `UUID` | `0` | Mesh asset UUID (`primitive://*` or `<model>?mesh=<i>`). |
| `albedoMapID` | `UUID` | `0` | PBR Albedo/Diffuse texture UUID bound to `PS t0` (`0` uses `1x1` white fallback). |
| `normalMapID` | `UUID` | `0` | Tangent-space normal map UUID bound to `PS t1` (`0` uses flat `(128, 128, 255)` fallback). |
| `ormMapID` | `UUID` | `0` | Channel-packed Ambient Occlusion (`R`), Roughness (`G`), Metallic (`B`) texture UUID bound to `PS t2`. |
| `matType` | `MaterialType` | `MaterialType::LitColor` | Shading mode (`LitColor`, `UnlitColor`, or `Textured`). |
| `baseColor` | `DirectX::XMFLOAT4` | `{1.0f, 1.0f, 1.0f, 1.0f}` | RGBA color multiplier (or solid color in `UnlitColor` mode). |
| `roughness` | `float` | `0.5f` | Scalar roughness `[0..1]` (multiplies `orm.g` in `BasicPS.hlsl`). |
| `metallic` | `float` | `0.0f` | Scalar metallic `[0..1]` (multiplies `orm.b` in `BasicPS.hlsl`). |

### `CameraComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `FOV` | `float` | `DirectX::XM_PIDIV4` (45°) | Vertical field of view in radians. |
| `nearClip` | `float` | `0.1f` | Perspective near clipping plane distance. |
| `farClip` | `float` | `500.0f` | Perspective far clipping plane distance (tuned to concentrate CSM shadow texel density). |
| `invertY` | `bool` | `true` | Vertical look inversion preference flag. |

### `ViewportComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `width` | `unsigned` | `1280` | Render viewport width in pixels (used for aspect ratio and 2D UI orthographic bounds). |
| `height` | `unsigned` | `720` | Render viewport height in pixels. |

### `EditorCamControlComponent`
Marks an entity as an Editor camera (hidden from the Hierarchy list and excluded when selecting a Play Mode game camera).

| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `mode` | `CameraControlMode` | `CameraControlMode::EditorCam` | Input control mode (`EditorCam` or `Scripted`). |
| `moveSpeed` | `float` | `10.0f` | Base WASD translation speed in units/second. |
| `lookSensitivity` | `float` | `0.0025f` | Radians per pixel of relative mouse movement. |
| `sprintMultiplier` | `float` | `2.0f` | Speed multiplier applied when `LShift` is held. |
| `yaw` | `float` | `0.0f` | Accumulated yaw angle in radians (`0` looks down `+Z`). |
| `pitch` | `float` | `0.0f` | Accumulated pitch angle in radians (clamped to ±89°). |

### `LightComponent`
Light direction for `Directional` and `Spot` lights is derived from the entity's `TransformComponent::rotation` (`+Z` rotated by the entity quaternion).

| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | Enables or disables light contribution. |
| `color` | `DirectX::XMFLOAT3` | `{1.0f, 1.0f, 1.0f}` | Linear RGB light color. |
| `intensity` | `float` | `1.0f` | Radiance multiplier. |
| `type` | `LightType` | `LightType::Directional` | `Directional` (`0`), `Point` (`1`), or `Spot` (`2`). |
| `range` | `float` | `10.0f` | Attenuation cutoff radius and frustum-culling bounding sphere radius (`Point` / `Spot`). |
| `spotAngle` | `float` | `DirectX::XM_PIDIV4` | Half-angle cone cutoff in radians for `Spot` lights (`theta <= cos(spotAngle)`). |

---

## Physics, Scripting, Audio & 2D UI Components

### `RigidBodyComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | When set to `false`, `PhysicsSystem` removes the Jolt body from the physics world. |
| `shape` | `RBShape` | `RBShape::Box` | Collider geometry (`Box`, `Sphere`, `Capsule`, or `Mesh`). |
| `motionType` | `RBMotion` | `RBMotion::Static` | `Static` (`NON_MOVING`) or `Dynamic` (`MOVING`). |
| `mass` | `float` | `1.0f` | Mass in kg (used when `motionType == RBMotion::Dynamic`). |
| `friction` | `float` | `0.5f` | Surface friction coefficient. |
| `restitution` | `float` | `0.0f` | Bounciness / elasticity coefficient. |
| `linearDamping` | `float` | `0.0f` | Linear velocity damping factor. |
| `halfExtent` | `DirectX::XMFLOAT3` | `{0.5f, 0.5f, 0.5f}` | Local half-extents used when `shape == RBShape::Box`. |
| `radius` | `float` | `0.5f` | Radius used when `shape` is `Sphere` or `Capsule`. |
| `height` | `float` | `1.0f` | Cylinder section height (`halfHeight = height * 0.5f`) used when `shape == RBShape::Capsule`. |
| `colliderScale` | `DirectX::XMFLOAT3` | `{1.0f, 1.0f, 1.0f}` | Additional scale multiplier applied on top of entity world scale when `shape == RBShape::Mesh`. |
| `meshID` | `UUID` | `0` | Source mesh UUID for `RBShape::Mesh` (auto-populated from `MeshRendererComponent::meshID` if `0`). |
| `bodyID` | `JPH::BodyID` | Invalid `BodyID` | Runtime Jolt body handle (not serialized). |
| `bodyCreated` | `bool` | `false` | Runtime registration flag (not serialized). |
| `showWireframe` | `bool` | `true` | Toggles green wireframe visualization in Edit mode when this entity is selected. |

### `ScriptInstance` & `LuaScriptComponent`
`LuaScriptComponent` holds `std::vector<ScriptInstance> scripts`, allowing an arbitrary number of isolated scripts on a single entity.

| `ScriptInstance` Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `filepath` | `std::string` | `""` | Relative path to the `.lua` script file (serialized under `"Scripts"`). |
| `env` | `sol::environment` | — | Per-instance sandboxed Lua environment containing `self` (runtime only). |
| `OnCreate` | `sol::function` | — | Cached lifecycle hook invoked on Play start or runtime attachment. |
| `OnUpdate` | `sol::function` | — | Cached per-frame hook invoked with `(dt)`. |
| `OnDestroy` | `sol::function` | — | Cached teardown hook invoked on entity destruction or Play stop. |
| `OnCollisionEnter` | `sol::function` | — | Cached physics contact-added callback `(other)`. |
| `OnCollisionExit` | `sol::function` | — | Cached physics contact-removed callback `(other)`. |
| `isInitialized` | `bool` | `false` | Tracks whether `env` has been built and `OnCreate` has executed. |

### `AudioComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `audioID` | `UUID` | `0` | Audio clip asset UUID (`.wav` / `.mp3`). |
| `is3D` | `bool` | `true` | `true` for 3D spatialized attenuation; `false` disables spatialization (`MA_SOUND_FLAG_NO_SPATIALIZATION`). |
| `loop` | `bool` | `false` | Loops playback continuously when `true`. |
| `playOnCreate` | `bool` | `true` | Automatically starts playback on the first Play Mode frame. |
| `volume` | `float` | `1.0f` | Gain multiplier (`[0.0f .. 1.0f]`). |
| `soundHandle` | `void*` | `nullptr` | Opaque runtime `AudioInstance*` managed by `AudioSystem` (not serialized). |
| `isPlaying` | `bool` | `false` | Runtime playback state flag (not serialized). |

### `SpriteRendererComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | Enables or disables 2D sprite rendering. |
| `textureID` | `UUID` | `0` | 2D texture UUID (`0` renders a solid white quad tinted by `tintColor`). |
| `position` | `DirectX::XMFLOAT2` | `{0.0f, 0.0f}` | Screen-space pixel coordinates (`(0, 0)` is top-left). |
| `size` | `DirectX::XMFLOAT2` | `{100.0f, 100.0f}` | Width and height in pixels. |
| `pivot` | `DirectX::XMFLOAT2` | `{0.0f, 0.0f}` | Normalized origin offset (`(0, 0)` top-left, `(0.5, 0.5)` center). |
| `rotation` | `float` | `0.0f` | Clockwise Z-axis rotation in radians (displayed in degrees in the Inspector). |
| `tintColor` | `DirectX::XMFLOAT4` | `{1.0f, 1.0f, 1.0f, 1.0f}` | RGBA color and alpha multiplier. |

### `TextRendererComponent`
| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `isActive` | `bool` | `true` | Enables or disables 2D text rendering. |
| `text` | `std::string` | `"New Text"` | Printable ASCII string (`32`–`126`) to render. |
| `fontID` | `UUID` | `0` | Baked `FontAsset` UUID (`<filepath>?size=<fontSize>`). |
| `fontSize` | `float` | `24.0f` | Glyph pixel height. |
| `position` | `DirectX::XMFLOAT2` | `{0.0f, 0.0f}` | Baseline start coordinates in screen-space pixels. |
| `color` | `DirectX::XMFLOAT4` | `{1.0f, 1.0f, 1.0f, 1.0f}` | RGBA text color and opacity. |
