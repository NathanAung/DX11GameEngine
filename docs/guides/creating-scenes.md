# Creating, Editing & Exporting Scenes

`DX11Editor` provides a dockable authoring workspace for assembling 3D worlds, tuning lighting and physics, playtesting in-editor, and exporting standalone executables.

---

## Editor Workspace & Controls

When `DX11Editor` launches, `EditorUI::SetupDockspace` arranges five dockable panels:

- **Toolbar** *(Top)* — **Play / Stop** state toggle and **Export Game** button.
- **Hierarchy** *(Left)* — Scene entity tree, context-menu spawning, and drag-and-drop parenting.
- **Scene Viewport** *(Center)* — Off-screen DirectX 11 render view, object picking, and `ImGuizmo` handles.
- **Inspector** *(Right)* — Active toggles, component properties, and **Add / Remove Component** menus.
- **Content Browser** *(Bottom)* — File explorer rooted at `assets/`, scene loader, and drag-and-drop asset source.

### Viewport Navigation & Shortcuts

| Input | Action |
| :--- | :--- |
| **Right Mouse Button (Hold)** *(in Scene panel)* | Captures cursor for free-look camera rotation |
| **`W` / `A` / `S` / `D`** *(while RMB held)* | Fly forward, left, backward, and right |
| **`Left Shift`** *(while flying, or while Scene focused)* | Multiplies fly speed by `sprintMultiplier` (`2.0x` default) |
| **Mouse Wheel** *(while Scene focused)* | Dolly camera forward / backward along view vector |
| **Left Mouse Button** *(in Scene panel)* | Raycast pick entity (Jolt narrow-phase first, OBB fallback) |
| **`Space`** *(while Scene focused in Edit mode)* | Cycle `ImGuizmo` mode: **Translate** -> **Rotate** -> **Scale** |
| **`Delete`** *(while Hierarchy focused)* | Destroy selected entity (cascades to all children) |
| **`Ctrl + S`** | Save current scene (opens **Save Scene As** modal if unsaved) |
| **`Escape`** | Close application |

---

## Step 1: Create a New Scene

1. Right-click empty space inside the **Content Browser** panel and select **Create New Scene**.
2. Enter a scene name (e.g., `Level01`) and click **Create**.
3. The editor invokes `Scene::GenerateDefaultSetup`, saves `assets/Scenes/Level01.json`, and updates the Scene tab title. Every new scene starts with four pre-configured entities:
    - **`Editor Camera`** *(hidden from Hierarchy)* — Fly camera used while authoring in `Edit` mode.
    - **`Main Camera`** — In-game camera automatically activated when entering `Play` mode or running a standalone export.
    - **`Sun Light`** — White `Directional` light (`intensity = 5.0`) pitched down at 45° to drive Cascaded Shadow Maps.
    - **`Ground`** — Static `20 x 0.1 x 20` box mesh and `RigidBodyComponent` at `(0, -5, 0)`.

---

## Step 2: Spawn & Organize Entities

### Spawning Built-In Entities

Right-click empty space in the **Hierarchy** panel and choose **Create New Entity**:

- **Empty Entity** — `IDComponent` + `NameComponent` + `TransformComponent` (ideal for hierarchy folders, script managers, audio emitters, or 2D HUD elements).
- **Camera** — Creates a new game `CameraComponent` + `ViewportComponent`.
- **Shapes** -> **Cube**, **Sphere**, or **Capsule** — Spawns a primitive `MeshRendererComponent`.
- **Lights** -> **Directional Light**, **Point Light**, or **Spot Light** — Spawns a `LightComponent` (up to 16 active frustum-visible lights render simultaneously; the first active `Directional` light casts 3-cascade shadows).

### Building Parent-Child Hierarchies

- **Parent an Entity**: Drag an entity node in the **Hierarchy** and drop it onto another entity node. Its world position, rotation, and scale remain unchanged while its `TransformComponent` converts into parent-relative local space.
- **Unparent to Root**: Drag a child entity node and drop it onto the empty space below the list in the **Hierarchy** panel.
- **Remove a Component**: Right-click any framed component header (`Light`, `Rigidbody`, `Mesh Renderer`, `Audio Emitter`, `Lua Scripts`, `Sprite Renderer`, `Text Renderer`) in the **Inspector** and click **Remove Component**.

---

## Step 3: Save & Load Scenes

### Saving

- Press **`Ctrl + S`**.
- If the scene has not been saved yet (such as the startup sandbox), the **Save Scene As** modal opens and writes `assets/Scenes/<Name>.json`.
- Subsequent **`Ctrl + S`** presses overwrite the active `.json` file and update `enginefiles/AssetRegistry.json` so any newly attached Lua scripts or imported assets are recorded.

### Loading

1. Ensure the Editor is in **Edit** mode (scenes cannot be loaded while **Play** mode is running).
2. In the **Content Browser**, navigate into `Scenes` (`assets/Scenes/`) and click `[SCENE] <Name>.json`.
3. `SceneSerializer::Deserialize` validates every asset UUID and script path before clearing the current scene, reconstructs all entities and hierarchies, and preloads referenced models and textures into VRAM.

### Scene JSON Structure Example

Scenes are stored in human-readable JSON so diffs remain clean in version control:

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

## Step 4: Playtest & Export a Standalone Game

### Non-Destructive Playtesting

1. Verify your scene has at least one active **Camera** entity (such as `Main Camera`).
2. Click **Play** in the top **Toolbar**:
    - The Editor snapshots the world (`CopyToBackup()`), hides authoring panels, switches to your game camera, runs `OnCreate()` on all Lua scripts, and begins stepping Jolt Physics and 3D audio.
3. Click **Stop** to invoke `OnDestroy()`, tear down runtime physics/audio handles, and restore the exact pre-play scene state.

### Exporting a Standalone Build (`ExportedGame/`)

1. **Prerequisite**: Build the `DX11Runtime` target (or **Build -> Build All** in Visual Studio) at least once so `${CMAKE_BINARY_DIR}/bin/RuntimeTemplate/` contains `DX11Runtime.exe` and its required `.dll` dependencies.
2. Save your startup scene (`Ctrl + S`) while in **Edit** mode.
3. Click **Export Game** in the **Toolbar**.
4. `AssetManager::ExportProject` automatically assembles `${WorkingDir}/ExportedGame/`:
    ```text
    ExportedGame/
    ├── DX11Runtime.exe          # Standalone game executable (DIST_BUILD)
    ├── *.dll                    # Required runtime DLLs (SDL2, assimp, lua, etc.)
    ├── data.pak                 # Packed PAK2 VFS archive (all assets, .cso shaders, scripts, scenes)
    └── enginefiles/
        └── Launch.txt           # Relative path to the exported startup scene JSON
    ```
5. Windows Explorer opens `ExportedGame/` automatically—double-click `DX11Runtime.exe` to run the standalone game!
