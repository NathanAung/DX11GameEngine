#include <fstream>
#include <iostream>
#include "Engine/Core.h"
#include "Engine/UUID.h"
#include "Engine/AssetManager.h"
#include "Engine/Renderer.h"
#include "Engine/InputManager.h"
#include "Engine/Scene.h"
#include "Engine/MeshManager.h"
#include "Engine/ShaderManager.h"
#include "Engine/Systems.h"
#include "Engine/TextureManager.h"
#include "Engine/AudioManager.h"
#include "Engine/SceneSerializer.h"
#ifndef DIST_BUILD
#include "Engine/ImGuiManager.h"
#include "Engine/EditorUI.h"
#endif // !DIST_BUILD




// Common Usings
using namespace DirectX;
using Engine::UUID;

// GLOBALS (app level)
SDL_Window* g_SDLWindow = nullptr;
HWND g_Hwnd = nullptr;

// Window dimensions
const int g_windowWidth = 1280;
const int g_windowHeight = 720;

// Timing variables
Uint64 g_perfFreq = 0;
Uint64 g_lastCounter = 0;
bool g_running = true;
bool g_vSync = true; // can toggle later

// Managers
Engine::AssetManager g_assetManager;
Engine::InputManager g_input;
Engine::MeshManager g_meshManager;
Engine::ShaderManager g_shaderManager;
Engine::TextureManager g_textureManager;
Engine::PhysicsManager g_physicsManager;
Engine::AudioManager g_audioManager;

// ECS: Scene and a sample 3d entity
Engine::Scene g_scene;
entt::entity g_sampleEntity = entt::null;

// Renderer
Engine::Renderer g_renderer;

#ifndef DIST_BUILD
// Editor UI
Engine::ImGuiManager g_imGuiManager;
Engine::EditorUI g_editorUI;
#endif // !DIST_BUILD

// Forward declarations
static void LoadCoreAssets();
static void LoadContent();
static void PreloadGameAssets();
void Update(float deltaTime);
void Render();


// Loads core assets that are required for the engine to function properly, such as default textures, shaders, and primitive meshes.
static void LoadCoreAssets()
{
    // Create the default 1x1 fallback texture
    g_textureManager.CreateDefaultTexture(g_renderer.GetDevice(), g_assetManager);

	// COMPILE & LOAD SHADERS
    const UUID shaderID = g_shaderManager.LoadBasicShaders(g_renderer.GetDevice(), g_assetManager); // Create resources with renderer device
    const UUID unlitShaderID = g_shaderManager.LoadUnlitShaders(g_renderer.GetDevice(), g_assetManager);
    const UUID skyboxShaderID = g_shaderManager.LoadSkyboxShaders(g_renderer.GetDevice(), g_assetManager);

    // Create shared primitive meshes
    const UUID cubeMeshID = g_meshManager.InitializeCube(g_renderer.GetDevice(), g_assetManager);
    const UUID sphereMeshID = g_meshManager.CreateSphere(g_renderer.GetDevice(), g_assetManager, 0.5f, 32, 32);
    const UUID capsuleMeshID = g_meshManager.CreateCapsule(g_renderer.GetDevice(), g_assetManager, 0.5f, 1.0f, 32, 32);

	// Cache default assets on the Scene so primitive spawning and editor UI can access them without needing to know the UUIDs
    g_scene.SetDefaultAssets(shaderID, unlitShaderID, skyboxShaderID, cubeMeshID, sphereMeshID, capsuleMeshID);

    // Load the default Skybox (order: +X, -X, +Y, -Y, +Z, -Z)
    std::vector<std::string> faces{
        "assets/Textures/Skybox/right.png", "assets/Textures/Skybox/left.png",
        "assets/Textures/Skybox/top.png", "assets/Textures/Skybox/bottom.png",
        "assets/Textures/Skybox/front.png", "assets/Textures/Skybox/back.png"
    };
    // Load the cubemap via TextureManager, which handles AssetManager registration and returns a UUID
    UUID skyID = g_textureManager.LoadCubemap(g_renderer.GetDevice(), g_assetManager, faces);
    if (skyID != 0) g_renderer.SetSkybox(g_textureManager.GetTexture(skyID));
    else throw std::runtime_error("Failed to load skybox cubemap textures.");
}


// Loads the main scene content, including cameras, lights, and a sample 3D model with physics.
static void LoadContent()
{
    // Create the editor camera entity
    g_scene.CreateEditorCamera("Editor Camera", g_renderer.GetWidth(), g_renderer.GetHeight());

	// Create a game camera entity (not controlled by editor, used for gameplay or scripted cameras)
	g_scene.CreateGameCamera("Main Camera", g_renderer.GetWidth(), g_renderer.GetHeight());

    // Create a directional light
    g_scene.CreateDirectionalLight("Sun Light");

    // Create a sample point light (red) near the model
    g_scene.CreatePointLight(
        "Red Point Light",
        XMFLOAT3{ 3.0f, -3.0f, -5.0f },     // position
        XMFLOAT3{ 1.0f, 0.2f, 0.2f },       // color (red)
        30.0f,                              // intensity
        40.0f                               // range
    );

    // Create a sample spot light (blue) aimed at the model from above
    {
        // calculate direction vector from position to target
        XMFLOAT3 spotPos{ 0.0f, -2.0f, 0.0f };
        XMFLOAT3 target{ 0.0f, -100.0f, 0.0f };
        XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&target), XMLoadFloat3(&spotPos)));
        XMFLOAT3 dirF{};
        XMStoreFloat3(&dirF, dir);

        g_scene.CreateSpotLight(
            "Blue Spot Light",
            spotPos,
            dirF,
            XMFLOAT3{ 0.2f, 0.4f, 1.0f },       // color (blue)
            100.0f,                             // intensity
            20.0f,                              // range
            XM_PIDIV4                           // 45 deg cone
        );
    }

    auto meshIDs = g_meshManager.LoadModel(g_renderer.GetDevice(), g_assetManager, "assets/Models/MyModel.obj");
    // Create the sample entity
    {
        g_sampleEntity = g_scene.CreateSampleEntity("Sample 3D Model");

        // Hook the sample entity to resources (now using first mesh from model)
        auto& mr = g_scene.registry.get<Engine::MeshRendererComponent>(g_sampleEntity);
        if (!meshIDs.empty())
        {
            UUID firstMeshID = meshIDs[0];
            mr.meshID = firstMeshID;
        }
        else {
            throw std::runtime_error("Failed to load model meshes.");
        }

		// example texture loading via TextureManager and AssetManager
        UUID texID = g_textureManager.LoadTexture(g_renderer.GetDevice(), g_assetManager, "assets/Textures/MyTexture.png");
        mr.textureID = texID; // assign texture to component
		mr.matType = Engine::MaterialType::Textured; // switch to textured shader path
        // PBR value testing
        mr.roughness = 0.3f; // shiny
        mr.metallic = 0.2f; // metallic (with yellow-ish albedo you'd get gold-like)

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Mesh;
        rb.motionType = Engine::RBMotion::Dynamic;
        rb.mass = 1.0f;
        rb.meshID = mr.meshID; // use same mesh for collider
        g_scene.registry.emplace<Engine::RigidBodyComponent>(g_sampleEntity, rb);
    }

	// PHYSICS TEST ENTITIES
    // Ground (static box)
    {
        entt::entity ground = g_scene.CreateEntity("Ground");
        auto& tc = g_scene.registry.get<Engine::TransformComponent>(ground);
        tc.position = XMFLOAT3(0.0f, -5.0f, 0.0f);
        tc.scale = XMFLOAT3(20.0f, 0.1f, 20.0f); // visual scaling to match collider

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Box;
        rb.motionType = Engine::RBMotion::Static;
        g_scene.registry.emplace<Engine::RigidBodyComponent>(ground, rb);

        Engine::MeshRendererComponent rend{};
        rend.meshID = g_scene.GetCubeMeshID();               // cube mesh
        rend.roughness = 0.1f;
        rend.metallic = 0.2f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(ground, rend);
    }

     // Falling Box (dynamic)
    {
        entt::entity box = g_scene.CreateEntity("Physics Box");
        auto& tc = g_scene.registry.get<Engine::TransformComponent>(box);
        tc.position = XMFLOAT3(1.0f, 20.0f, 2.0f);
        tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Box;
        rb.motionType = Engine::RBMotion::Dynamic;
        rb.mass = 1.0f;
        g_scene.registry.emplace<Engine::RigidBodyComponent>(box, rb);

        Engine::MeshRendererComponent rend{};
        rend.meshID = g_scene.GetCubeMeshID();
        rend.roughness = 0.1f;
        rend.metallic = 0.2f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(box, rend);
    }

    // Falling Sphere (dynamic)
    {
        entt::entity sphere = g_scene.CreateEntity("Physics Sphere");
        auto& tc = g_scene.registry.get<Engine::TransformComponent>(sphere);
        tc.position = XMFLOAT3(0.5f, 20.0f, 2.0f);
        tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Sphere;
        rb.motionType = Engine::RBMotion::Dynamic;
        rb.mass = 1.0f;
        rb.radius = 0.5f;
		rb.restitution = 0.5f; // bouncy
        g_scene.registry.emplace<Engine::RigidBodyComponent>(sphere, rb);

        Engine::MeshRendererComponent rend{};
		rend.meshID = g_scene.GetSphereMeshID();
        rend.roughness = 0.1f;
        rend.metallic = 0.2f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(sphere, rend);
    }

    // Falling Capsule (dynamic)
    {
        entt::entity capsule = g_scene.CreateEntity("Physics Capsule");
        auto& tc = g_scene.registry.get<Engine::TransformComponent>(capsule);
        tc.position = XMFLOAT3(2.0f, 10.0f, 2.0f);
        tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Capsule;
        rb.motionType = Engine::RBMotion::Dynamic;
        rb.mass = 1.0f;
        rb.radius = 0.5f;
        rb.height = 1.0f; // cylinder height in PhysicsManager logic
        g_scene.registry.emplace<Engine::RigidBodyComponent>(capsule, rb);

        Engine::MeshRendererComponent rend{};
		rend.meshID = g_scene.GetCapsuleMeshID();
        rend.roughness = 0.1f;
        rend.metallic = 0.2f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(capsule, rend);
    }
}


// Preloads all game assets into VRAM to reduce runtime loading stutter. This is especially useful for large models and textures.
static void PreloadGameAssets()
{
    // Snapshot the assets we need to load to avoid iterator invalidation
    std::vector<Engine::AssetMetadata> loadSnapshot;

    for (const auto& [uuid, meta] : g_assetManager.GetRegistry())
    {
        // Only snapshot the types we actively want to push to VRAM
        if (meta.type == Engine::AssetType::ModelFile || meta.type == Engine::AssetType::Texture)
        {
            loadSnapshot.push_back(meta);
        }
    }

	// Iterate safely through the isolated snapshot and push physical files to VRAM
    for (const auto& meta : loadSnapshot)
    {
		// Skip .mtl files since they are loaded automatically by Assimp when loading the corresponding .obj model
        if (meta.type == Engine::AssetType::ModelFile && meta.filepath.find(".mtl") == std::string::npos) {
            g_meshManager.LoadModel(g_renderer.GetDevice(), g_assetManager, meta.filepath);
        }
        else if (meta.type == Engine::AssetType::Texture){
            if (meta.filepath.find("primitive://") == std::string::npos && meta.filepath.find("cubemap://") == std::string::npos){
                g_textureManager.LoadTexture(g_renderer.GetDevice(), g_assetManager, meta.filepath);
            }
        }
    }
}


// Main entry point
int main(int argc, char** argv)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // Create SDL window suitable for DirectX
    g_SDLWindow = SDL_CreateWindow(
        "DX11GameEngine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        g_windowWidth, g_windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!g_SDLWindow)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Get HWND from SDL window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(g_SDLWindow, &wmInfo))
    {
        std::fprintf(stderr, "SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_SDLWindow);
        SDL_Quit();
        return -1;
    }
    g_Hwnd = wmInfo.info.win.window;

    // Initialize DirectX 11 via Renderer
    if (!g_renderer.InitD3D11(g_Hwnd, (UINT)g_windowWidth, (UINT)g_windowHeight))
    {
        std::fprintf(stderr, "Renderer initialization failed\n");
        SDL_DestroyWindow(g_SDLWindow);
        SDL_Quit();
        return -1;
    }

#ifndef DIST_BUILD
    // Initialize Dear ImGui (SDL2 + DX11) after D3D11 is ready
    if (!g_imGuiManager.Initialize(g_SDLWindow, g_renderer.GetDevice(), g_renderer.GetContext()))
    {
        std::fprintf(stderr, "ImGuiManager initialization failed\n");
        g_renderer.Shutdown();
        if (g_SDLWindow) {
            SDL_DestroyWindow(g_SDLWindow);
            g_SDLWindow = nullptr;
        }
        SDL_Quit();
        return -1;
    }
#endif // !DIST_BUILD

    // Initialize physics (Jolt)
    if (!g_physicsManager.Initialize())
    {
        std::fprintf(stderr, "PhysicsManager initialization failed\n");

#ifndef DIST_BUILD
        g_imGuiManager.Shutdown();
#endif // !DIST_BUILD

        g_renderer.Shutdown();
        if (g_SDLWindow) {
            SDL_DestroyWindow(g_SDLWindow);
            g_SDLWindow = nullptr;
        }
        SDL_Quit();
        return -1;
    }

    // Initialize Audio
    if (!g_audioManager.Initialize())
    {
        std::fprintf(stderr, "AudioManager initialization failed\n");
        g_physicsManager.Shutdown();

#ifndef DIST_BUILD
        g_imGuiManager.Shutdown();
#endif // !DIST_BUILD

        g_renderer.Shutdown();
        if (g_SDLWindow) {
            SDL_DestroyWindow(g_SDLWindow);
            g_SDLWindow = nullptr;
        }
        SDL_Quit();
        return -1;
    }

    // RESTORE THE ASSET LEDGER
    // NOTE: Do this BEFORE LoadContent() so primitive generation reuses the saved UUIDs
    g_assetManager.LoadRegistry("enginefiles/AssetRegistry.json");

    try {
#ifdef DIST_BUILD
        // GAME MODE:
        // Mount the packed binary archive first before attempting to load any assets
        if (!g_assetManager.MountVFS("data.pak"))
        {
            throw std::runtime_error("Failed to mount data.pak archive! Ensure it was exported correctly.");
        }
#endif
        // Always load core shaders and primitives, regardless of build type
        LoadCoreAssets();

#ifndef DIST_BUILD
        // EDITOR MODE: Load the testing sandbox content
        LoadContent();
#else
        // GAME MODE:
        // Pre-load all external assets from the registry into VRAM
        PreloadGameAssets();

        // Read the launch configuration to find the starting scene
        std::string startScenePath;
        std::ifstream launchFile("enginefiles/Launch.txt");
        if (launchFile.is_open()) {
            std::getline(launchFile, startScenePath);
            launchFile.close();
        }
        else {
            throw std::runtime_error("Could not find enginefiles/Launch.txt. The game doesn't know which scene to load.");
        }

        // Automatically load the designated start scene and enter Play Mode immediately
        std::string errorMsg;
        bool loaded = Engine::SceneSerializer::Deserialize(startScenePath, g_scene, g_physicsManager, g_assetManager, errorMsg);

        if (!loaded) {
            throw std::runtime_error("Failed to load " + startScenePath + ": " + errorMsg);
        }

        // Snapshot the clean scene and immediately boot Script and Physics systems
        g_scene.CopyToBackup();
        Engine::ScriptSystemInit(g_scene);

        // Find the active Game Camera to look through
        auto camView = g_scene.registry.view<Engine::CameraComponent>();
        for (auto entity : camView)
        {
            if (!g_scene.registry.all_of<Engine::EditorCamControlComponent>(entity) && g_scene.registry.get<Engine::NameComponent>(entity).isActive)
            {
                g_scene.m_activeRenderCamera = entity;

                // Force the viewport component to match the full window size
                auto& vp = g_scene.registry.get<Engine::ViewportComponent>(entity);
                vp.width = g_windowWidth;
                vp.height = g_windowHeight;
                break;
            }
        }
#endif
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Content load failed: %s\n", e.what());
		// pause and wait for user input before closing the console window (useful for debugging)
		std::fprintf(stderr, "Press Enter to exit...");
		std::cin.get();

        g_audioManager.Shutdown();
        g_physicsManager.Shutdown();

#ifndef DIST_BUILD
        g_imGuiManager.Shutdown();
#endif // !DIST_BUILD

        g_renderer.Shutdown();
        if (g_SDLWindow) {
            SDL_DestroyWindow(g_SDLWindow);
            g_SDLWindow = nullptr;
        }
        SDL_Quit();
        return -1;
    }

    // Main loop
    g_perfFreq = SDL_GetPerformanceFrequency();
    g_lastCounter = SDL_GetPerformanceCounter();

	// Set the AudioManager reference in the Scene for audio playback and ECS integration
    g_scene.SetAudioManager(&g_audioManager);
	// Set the AssetManager reference in the Scene for asset lookups and spawning
    g_scene.SetAssetManager(&g_assetManager);

	// Initialize Lua bindings with access to input and physics manager
    g_scene.InitializeLuaBindings(&g_input, &g_physicsManager);

    while (g_running)
    {
        // Begin input frame
        g_input.BeginFrame();

        // Event handling
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            // Global input handling (e.g., exit on Escape key)
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) { g_running = false; }

#ifndef DIST_BUILD
            // Intercept events for Dear ImGui first
            bool imguiCaptured = g_imGuiManager.ProcessEvent(e);

            // Feed the input manager if ImGui didn't capture this frame's input
            // When right-click flying in the Scene view, ImGui will capture input.
            // mouse delta + WASD is still needed to reach the engine while captured.
            if (!imguiCaptured || g_input.IsMouseCaptured() || g_editorUI.IsSceneFocused() || e.type == SDL_KEYUP)
            {
                g_input.ProcessEvent(e);
            }
#else
            // In a game build, all input goes straight to the engine
            g_input.ProcessEvent(e);
#endif // !DIST_BUILD



            if (e.type == SDL_QUIT) g_running = false;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                (void)g_renderer.Resize((UINT)e.window.data1, (UINT)e.window.data2);
                // NOTE: Camera ViewportComponent updates are now handled by the "Scene" ImGui panel sizing.

#ifdef DIST_BUILD
// In Game Mode, resizing the window must resize the active camera viewport
                if (g_scene.m_activeRenderCamera != entt::null && g_scene.registry.all_of<Engine::ViewportComponent>(g_scene.m_activeRenderCamera)) {
                    auto& vp = g_scene.registry.get<Engine::ViewportComponent>(g_scene.m_activeRenderCamera);
                    vp.width = (UINT)e.window.data1;
                    vp.height = (UINT)e.window.data2;
                }
#endif
            }
        }

        // Update
        Uint64 currentCounter = SDL_GetPerformanceCounter();
        float dt = float(double(currentCounter - g_lastCounter) / double(g_perfFreq));    // delta time in seconds
        g_lastCounter = currentCounter;

        Update(dt);

        // Render & present
        Render();
    }

    // Save all newly discovered UUIDs to the disk before shutting down
    g_assetManager.SaveRegistry("enginefiles/AssetRegistry.json");

    // Shutdown and cleanup
    g_audioManager.Shutdown();
    g_physicsManager.Shutdown();

#ifndef DIST_BUILD
    g_imGuiManager.Shutdown();
#endif // !DIST_BUILD

    g_renderer.Shutdown();
    if (g_SDLWindow) {
        SDL_DestroyWindow(g_SDLWindow);
        g_SDLWindow = nullptr;
    }
    SDL_Quit();
    return 0;
}

void Update(float deltaTime) {

#ifndef DIST_BUILD
	// In editor mode, only simulate physics and scripts when in Play mode
    bool isPlaying = g_editorUI.GetState() == Engine::EditorState::Play;
#else
    // In a game build, always simulate physics and scripts
	bool isPlaying = true;
#endif // !DIST_BUILD

    // Physics step and sync (Play: simulate + pull. Edit: push gizmo transforms to colliders)
    Engine::PhysicsSystem(g_scene, g_physicsManager, g_meshManager, deltaTime, isPlaying);

    if (isPlaying) {
        Engine::ScriptSystemUpdate(g_scene, deltaTime);

        Engine::AudioSystem(g_scene, g_audioManager, g_assetManager);

        // Safely destroy any entities that scripts asked to kill this frame
        g_scene.ProcessDestructionQueue(g_physicsManager);
    }

#ifndef DIST_BUILD
    // only process editor camera in Edit mode
    if (g_editorUI.GetState() == Engine::EditorState::Edit)
        Engine::EditorCameraInputSystem(g_scene, g_input, deltaTime, g_editorUI.IsSceneFocused());
#endif // !DIST_BUILD

    // Execute the transform hierarchy update after input/gizmo/physics changes
    Engine::TransformSystem(g_scene);

    Engine::CameraMatrixSystem(g_scene, g_renderer);
    //Engine::DemoRotationSystem(g_scene, g_sampleEntity, deltaTime);
}

void Render()
{
#ifndef DIST_BUILD
    // Start the ImGui frame (after processing input and before rendering)
    g_imGuiManager.BeginFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (g_input.IsMouseCaptured())
    {
        // Tell ImGui to completely ignore all mouse inputs while the camera is active
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    else
    {
        // Restore mouse inputs when the camera is released
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    // Render the editor UI (ImGui panels, etc.) first to set up the framebuffer and any UI state
    g_editorUI.Render(g_scene, g_renderer, g_meshManager, g_textureManager, g_input, g_physicsManager, g_SDLWindow);

    // Render the 3D scene into the off-screen framebuffer (Render-to-Texture)
    g_renderer.BindFramebuffer();
#else
    // In a game build, render directly to the back buffer
	g_renderer.BindBackBuffer();
#endif // !DIST_BUILD



    Engine::RenderSystem::DrawEntities(g_scene, g_meshManager, g_shaderManager, g_renderer, g_textureManager);

#ifndef DIST_BUILD
    // Draw debug colliders if in Edit mode
    if (g_editorUI.GetState() == Engine::EditorState::Edit)
        Engine::RenderSystem::DrawDebugColliders(g_scene, g_renderer, g_meshManager, g_shaderManager, g_editorUI.GetSelectedEntity());
#endif // !DIST_BUILD

    // Draw skybox last: z=w ensures it renders only where nothing else drew
    if (g_scene.m_activeRenderCamera != entt::null &&
        g_scene.registry.valid(g_scene.m_activeRenderCamera) &&
        g_scene.registry.all_of<Engine::TransformComponent, Engine::CameraComponent>(g_scene.m_activeRenderCamera))
    {
        const auto& camTrans = g_scene.registry.get<Engine::TransformComponent>(g_scene.m_activeRenderCamera);
        const auto& camComp = g_scene.registry.get<Engine::CameraComponent>(g_scene.m_activeRenderCamera);
        g_renderer.DrawSkybox(g_meshManager, g_shaderManager, camComp, camTrans, g_scene.GetCubeMeshID(), g_scene.GetSkyboxShaderID());
    }

#ifndef DIST_BUILD
    // Now bind the real swapchain back buffer.
    // NOTE: The window will intentionally render black until ImGui displays the framebuffer SRV.
    g_renderer.BindBackBuffer();
    // Draw the UI data to the cleared backbuffer
    g_imGuiManager.EndFrame();
#endif // !DIST_BUILD

    g_renderer.Present(g_vSync);
}