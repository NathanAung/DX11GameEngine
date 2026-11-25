# Adding & Managing Assets

Step Engine organizes project content inside the `assets/` directory and tracks every imported resource using a persistent 64-bit `Engine::UUID` stored in `enginefiles/AssetRegistry.json`.

---

## Asset Directory Conventions & Supported Formats

During CMake builds, the `CopyAssets` target copies the root `assets/` folder into `${CMAKE_BINARY_DIR}/bin/assets/` (the working directory of `DX11Editor` in Visual Studio).

> **Important Working Directory Tip:** When running `DX11Editor` from Visual Studio, the working directory is `${CMAKE_BINARY_DIR}/bin` (e.g., `out/build/x64-Debug/bin`). Place new assets inside the repository's `assets/` folder so they remain tracked in Git and get copied on build—or copy them into both `assets/` and `bin/assets/` if adding files while the editor is already open.

| Subdirectory | Supported Extensions | Drag-and-Drop Payload | Used By |
| :--- | :--- | :--- | :--- |
| `assets/Models/` | `.obj` (+ `.mtl`), `.fbx`, `.gltf` (+ `.bin`), `.glb` | `MODEL_FILE` | `MeshManager`, `Scene::InstantiateModel`, `MeshRendererComponent`, `RigidBodyComponent` (`Mesh`) |
| `assets/Textures/` | `.png`, `.jpg`, `.jpeg` | `TEXTURE_FILE` | `TextureManager`, `MeshRendererComponent` (Albedo / Normal / ORM), `SpriteRendererComponent` |
| `assets/Audio/` | `.wav`, `.mp3` | `AUDIO_FILE` | `AudioManager`, `AudioComponent` |
| `assets/Fonts/` | `.ttf` | `FONT_FILE` | `FontManager`, `TextRendererComponent` |
| `assets/Scripts/` | `.lua` | `SCRIPT_FILE` | `LuaScriptComponent`, `ScriptSystem` |
| `assets/Scenes/` | `.json` | *(Click to load)* | `SceneSerializer`, `EditorUI` Content Browser |

*(Windows paths are case-insensitive, so `assets/Audio` and `assets/audio` resolve identically on Windows, though keeping consistent relative paths with forward slashes `/` is recommended for VFS compatibility.)*

---

## 1. Importing 3D Models

### Via the Editor (Drag-and-Drop)

1. Place your model file (and any companion `.mtl` or `.bin` files) under `assets/Models/<YourModel>/`.
2. Open the **Content Browser** panel at the bottom of `DX11Editor` and navigate to the model file.
3. Drag the `.obj`, `.fbx`, `.gltf`, or `.glb` entry from the **Content Browser** into the empty space at the bottom of the **Hierarchy** panel.
4. `Scene::InstantiateModel` automatically:
    - Parses the file with Assimp (`Triangulate`, `FlipUVs`, `MakeLeftHanded`, `FlipWindingOrder`, `PreTransformVertices`, `CalcTangentSpace`).
    - Registers the container file (`AssetType::ModelFile`), any `.mtl`/`.bin` sidecars opened through `VFSIOSystem`, and each sub-mesh (`<filepath>?mesh=<i>`).
    - Creates a root parent entity named after the file stem (e.g., `ABeautifulGame`) and attaches child entities (`ABeautifulGame_mesh_0`, `ABeautifulGame_mesh_1`, ...) each equipped with a `MeshRendererComponent`.

### Adding a Mesh Collider

To give an imported sub-mesh physical collision:

1. Select the child sub-mesh entity in the **Hierarchy**.
2. In the **Inspector**, click **Add Component -> Rigidbody**.
3. Change **Shape** to `Mesh`:
    - Choose **Motion Type** `Static` for level geometry or props that do not move (builds an exact concave `JPH::MeshShape`).
    - Choose **Motion Type** `Dynamic` for movable physics objects (automatically builds a `JPH::ConvexHullShape`).
4. `PhysicsSystem` automatically wires `RigidBodyComponent::meshID` from the entity's `MeshRendererComponent::meshID`. Adjust **Collider Scale** if needed and verify alignment using the **Show Wireframe** green overlay.

### Via Lua at Runtime

Attach a custom `MeshRendererComponent` and assign a specific sub-mesh index (`?mesh=0`, `?mesh=1`, etc.):

```lua
self:AddMeshRenderer(MeshType.Custom)
self:SetMesh("assets/Models/container/model/container.obj?mesh=0")
```

---

## 2. Assigning PBR Materials & Textures

`MeshRendererComponent` uses a metallic-roughness PBR workflow when **Material Type** is set to `Textured`.

### Texture Channel Packing Rules

| Slot | Shader Register | Expected Format & Channels | Default Fallback (if cleared / `0`) |
| :--- | :--- | :--- | :--- |
| **Albedo (Color)** | `t0` | RGBA base color map (multiplied by `Base Color` tint) | `1x1` Pure White `(255, 255, 255, 255)` |
| **Normal Map** | `t1` | Tangent-space DirectX normal map (`+Y` down / DirectX convention) | `1x1` Flat Z-Up `(128, 128, 255, 255)` |
| **ORM Map** | `t2` | **R** = Ambient Occlusion, **G** = Roughness, **B** = Metallic | `1x1` `(255, 128, 0, 255)` (AO=1, Rough=0.5, Metal=0) |

> **Roughness & Metallic Multipliers:** In `BasicPS.hlsl`, sampled ORM channels are multiplied by the component's scalar sliders (`roughness = orm.g * g_Roughness`, `metallic = orm.b * g_Metallic`). When using an ORM map that encodes metallic detail in the Blue channel, set the Inspector **Metallic** slider above `0.0` (e.g., `1.0`) so metallic regions are not multiplied to zero!

### Steps in the Inspector

1. Select an entity with a **Mesh Renderer** component.
2. Change **Material Type** to `Textured`.
3. Drag `.png` or `.jpg` files from the **Content Browser** onto the **Albedo (Color)**, **Normal Map**, and **ORM** drop zones (or click `X` next to any slot to revert to its 1x1 fallback texture).
4. Tune the **Roughness** and **Metallic** sliders to scale the surface response.

---

## 3. Adding 2D Sprites & TrueType Fonts (HUD / UI)

### 2D Sprites

1. Select an entity (or create an **Empty Entity** in the Hierarchy for HUD elements).
2. Click **Add Component -> Sprite Renderer**.
3. Drag a `.png` / `.jpg` image from the **Content Browser** onto the **Sprite Texture** drop zone.
4. Set screen-space **Position (X,Y)** and **Size (W,H)** in pixels (`(0, 0)` is the top-left of the viewport), **Pivot (0-1)** (`(0.5, 0.5)` centers the sprite), **Rotation** (degrees), and **Tint Color**.

### Dynamic Text & `.ttf` Fonts

1. Place `.ttf` font files inside `assets/Fonts/` (e.g., `assets/Fonts/FiraCode-VariableFont_wght.ttf`).
2. Select an entity and click **Add Component -> Text Renderer**.
3. Select your `.ttf` file from the **Font** dropdown in the Inspector.
4. Adjust **Font Size** (`8`–`256` px), **Text**, **Position (X,Y)**, and **Color**.
    - Changing the font file or size automatically bakes a `2048x2048` RGBA glyph atlas (`stb_truetype`, ASCII `32`–`126`) under the virtual asset path `<filepath>?size=<fontSize>`.

---

## 4. Adding Audio Clips

1. Place `.wav` or `.mp3` files inside `assets/Audio/`.
2. Select an entity in the **Hierarchy** and click **Add Component -> Audio Component**.
3. Choose the audio clip from the **Audio File** dropdown, or drag the `.wav`/`.mp3` file from the **Content Browser** onto the combo box.
4. Configure playback behavior:
    - **Is 3D**: Enables 3D distance attenuation and panning relative to the active camera listener. Disable (`false`) for 2D background music or UI sound effects.
    - **Looping**: Repeats continuously while playing.
    - **Play On Create**: Automatically starts playback as soon as Play Mode begins.
    - **Volume**: Clamped between `0.0` (mute) and `1.0` (full gain).

---

## 5. Customizing the Skybox

By default, `LoadCoreAssets()` in `src/main.cpp` loads six cubemap faces in DirectX `+X, -X, +Y, -Y, +Z, -Z` order:

1. `assets/Textures/Skybox/right.png` (`+X`)
2. `assets/Textures/Skybox/left.png` (`-X`)
3. `assets/Textures/Skybox/top.png` (`+Y`)
4. `assets/Textures/Skybox/bottom.png` (`-Y`)
5. `assets/Textures/Skybox/front.png` (`+Z`)
6. `assets/Textures/Skybox/back.png` (`-Z`)

All six images must share identical pixel dimensions. You can replace those six `.png` files directly or pass a custom 6-element vector to `TextureManager::LoadCubemap` and `Renderer::SetSkybox`.

---

## How `AssetRegistry.json` & VFS Packing Work

- Whenever `AssetManager::ImportAsset` encounters a new file path, it generates a random 64-bit `UUID`.
- Saving a scene (`Ctrl + S`) or closing `DX11Editor` writes all known `(UUID, Filepath, Type)` entries to `enginefiles/AssetRegistry.json`.
- Do **not** delete `enginefiles/AssetRegistry.json` once you have saved `.json` scenes under `assets/Scenes/`—scene files reference meshes, textures, fonts, and audio clips by their 64-bit `UUID`s! If an asset referenced by a scene is missing from the registry or deleted from disk, `SceneSerializer::Deserialize` safely aborts loading and displays a **Scene Validation Failed** modal explaining which asset type is missing.
