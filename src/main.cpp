#include "Engine/Core.h"
#include "Engine/Renderer.h"
#include "Engine/InputManager.h"
#include "Engine/Scene.h"
#include "Engine/MeshManager.h"
#include "Engine/ShaderManager.h"
#include "Engine/Systems.h"
#include "Engine/TextureManager.h"
#include "Engine/ImGuiManager.h"
#include "Engine/MathUtils.h"
#include <filesystem>

// Common Usings
using namespace DirectX;

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

// Input manager
Engine::InputManager g_input;

// Track Scene panel focus for smart input routing (Shift+WASD without RMB)
bool g_scenePanelFocused = false;

// ECS: Scene and a sample 3d entity
Engine::Scene g_scene;
entt::entity g_sampleEntity = entt::null;
// Track selected entity in the Hierarchy panel for property editing
entt::entity g_selectedEntity = entt::null; 

// Content Browser state
const std::filesystem::path g_assetPath = "assets";         // Root directory that the content browser won't navigate above
std::filesystem::path g_currentDirectory = g_assetPath;     // Tracks the folder the user is currently viewing

// Managers
Engine::MeshManager g_meshManager;
Engine::ShaderManager g_shaderManager;
Engine::TextureManager g_textureManager; // global texture manager instance

// Renderer
Engine::Renderer g_renderer;

// Physics
Engine::PhysicsManager g_physicsManager;

// ImGui Manager
Engine::ImGuiManager g_imGuiManager;

// Forward declarations
static void LoadContent();
void Update(float deltaTime);
void Render();

static void LoadContent()
{
    // Create the default 1x1 fallback texture
    g_textureManager.CreateDefaultTexture(g_renderer.GetDevice());

    // Create resources with renderer device
    const int shaderID   = g_shaderManager.LoadBasicShaders(g_renderer.GetDevice());

    // Ensure skybox cube mesh (ID 101) always exists for DrawSkybox
    // Note: keep this unconditional to guarantee Mesh 101 availability
    const int cubeMeshID = g_meshManager.InitializeCube(g_renderer.GetDevice()); // temporary ID 101

    // Compile & load skybox shaders (assign temporary ID 2 inside ShaderManager implementation)
    const int skyboxShaderID = g_shaderManager.LoadSkyboxShaders(g_renderer.GetDevice());

    // Create the editor camera entity
    g_scene.CreateEditorCamera("Main Editor Camera", g_renderer.GetWidth(), g_renderer.GetHeight());

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

    auto meshIDs = g_meshManager.LoadModel(g_renderer.GetDevice(), "assets/Models/MyModel.obj");
    // Create the sample entity
    {
        g_sampleEntity = g_scene.CreateSampleEntity("Sample 3D Model");

        // Hook the sample entity to resources (now using first mesh from model)
        auto& mr = g_scene.registry.get<Engine::MeshRendererComponent>(g_sampleEntity);
        if (!meshIDs.empty())
        {
            int firstMeshID = meshIDs[0];
            mr.meshID = firstMeshID;
        }
        else {
            throw std::runtime_error("Failed to load model meshes.");
        }
        //else
        //{
        //    // fallback to temp cube if model failed to load
        //    mr.meshID = 101;    // per spec, temporary ID
        //}

        mr.materialID = shaderID;  // map materialID -> shaderID(1) (temporary ID)

        // example texture loading via texture manager and keep SRV
        ID3D11ShaderResourceView* tex = g_textureManager.LoadTexture(g_renderer.GetDevice(), "assets/Textures/MyTexture.png");
        mr.texture = tex; // assign texture to component
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

    // Load skybox cubemap: order +X, -X, +Y, -Y, +Z, -Z
    {
        std::vector<std::string> faces{
            "assets/Textures/Skybox/right.png",  // +X
            "assets/Textures/Skybox/left.png",   // -X
            "assets/Textures/Skybox/top.png",    // +Y
            "assets/Textures/Skybox/bottom.png", // -Y
            "assets/Textures/Skybox/front.png",  // +Z
            "assets/Textures/Skybox/back.png"    // -Z
        };
        if (ID3D11ShaderResourceView* skySRV = g_textureManager.LoadCubemap(g_renderer.GetDevice(), faces))
        {
            g_renderer.SetSkybox(skySRV);
			//std::printf("Skybox cubemap loaded successfully.\n");
        }
        else {
			throw std::runtime_error("Failed to load skybox cubemap textures.");
        }
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
        rend.meshID = 101;               // cube mesh
        rend.materialID = shaderID;
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
        rend.meshID = 101;
        rend.materialID = shaderID;
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
        rend.meshID = g_meshManager.CreateSphere(g_renderer.GetDevice(), 0.5f, 32, 32); // radius matches physics
        rend.materialID = shaderID;
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
        rend.meshID = g_meshManager.CreateCapsule(g_renderer.GetDevice(), 0.5f, 1.0f, 32, 32);
        rend.materialID = shaderID;
        rend.roughness = 0.1f;
        rend.metallic = 0.2f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(capsule, rend);
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

    // Initialize physics (Jolt)
    if (!g_physicsManager.Initialize())
    {
        std::fprintf(stderr, "PhysicsManager initialization failed\n");
        g_imGuiManager.Shutdown();
        g_renderer.Shutdown();
        if (g_SDLWindow) {
            SDL_DestroyWindow(g_SDLWindow);
            g_SDLWindow = nullptr;
        }
        SDL_Quit();
        return -1;
    }

    try {
        LoadContent();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Content load failed: %s\n", e.what());
        g_physicsManager.Shutdown();
        g_imGuiManager.Shutdown();
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

            // Intercept events for Dear ImGui first
            bool imguiCaptured = g_imGuiManager.ProcessEvent(e);

            // Feed the input manager if ImGui didn't capture this frame's input
            // When right-click flying in the Scene view, ImGui will capture input.
            // mouse delta + WASD is still needed to reach the engine while captured.
            if (!imguiCaptured || g_input.IsMouseCaptured() || g_scenePanelFocused || e.type == SDL_KEYUP)
            {
                g_input.ProcessEvent(e);
            }

            if (e.type == SDL_QUIT) g_running = false;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                (void)g_renderer.Resize((UINT)e.window.data1, (UINT)e.window.data2);
                // NOTE: Camera ViewportComponent updates are now handled by the "Scene" ImGui panel sizing.
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

    // Shutdown and cleanup
    g_physicsManager.Shutdown();
    g_imGuiManager.Shutdown();
    g_renderer.Shutdown();
    if (g_SDLWindow) {
        SDL_DestroyWindow(g_SDLWindow);
        g_SDLWindow = nullptr;
    }
    SDL_Quit();
    return 0;
}

void Update(float deltaTime) {
    // Physics step and sync
    Engine::PhysicsSystem(g_scene, g_physicsManager, g_meshManager, deltaTime);

    Engine::CameraInputSystem(g_scene, g_input, deltaTime, g_scenePanelFocused);

    Engine::CameraMatrixSystem(g_scene, g_renderer);
    //Engine::DemoRotationSystem(g_scene, g_sampleEntity, deltaTime);
}

void Render()
{
	// Start the ImGui frame (after processing input and before rendering)
    g_imGuiManager.BeginFrame();

    // Root editor layout: invisible grid that panels dock into
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

    // Set a default size (e.g., 800x600) only if there is no saved setting in imgui.ini
    ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);
	// set the window to be top centered on first use as well
	ImGui::SetNextWindowPos(ImVec2((g_windowWidth - 800) / 2.0f, 20.0f), ImGuiCond_FirstUseEver);

	// SCENE WINDOW
    // Scene View (dockable): drives the render-to-texture size
    ImGui::Begin("Scene");
	// Get the available size for the viewport (this is the size of the content region inside the "Scene" window)
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	// Update the active camera's viewport size if it doesn't match the current viewport size
    if (g_scene.m_activeRenderCamera != entt::null &&
        g_scene.registry.valid(g_scene.m_activeRenderCamera) &&
        g_scene.registry.all_of<Engine::ViewportComponent>(g_scene.m_activeRenderCamera))
    {
        auto& vp = g_scene.registry.get<Engine::ViewportComponent>(g_scene.m_activeRenderCamera);

        if (viewportSize.x != vp.width || viewportSize.y != vp.height)
        {
            if (viewportSize.x > 0.0f && viewportSize.y > 0.0f)
            {
                vp.width  = viewportSize.x;
                vp.height = viewportSize.y;

                g_renderer.CreateFramebuffer((UINT)viewportSize.x, (UINT)viewportSize.y);
            }
        }
    }

	// Render the framebuffer texture (from off-screen rendering) as an ImGui image in the Scene panel
    ImGui::Image((ImTextureID)(intptr_t)g_renderer.GetFramebufferSRV(), viewportSize);

    // Smart Input Routing: Right-Click to Fly (Scene panel only)
    {
		// Check if the Scene panel is hovered for input routing
        bool isHovered = ImGui::IsWindowHovered();
        g_scenePanelFocused = ImGui::IsWindowFocused();

		// Store mouse position on RMB down to restore it on release (prevents warping issues if cursor leaves panel while flying)
        static int storedMouseX = 0;
        static int storedMouseY = 0;

		// When right mouse button is clicked while hovering the Scene panel, capture the mouse for camera control
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            SDL_GetMouseState(&storedMouseX, &storedMouseY);
            g_input.SetMouseCaptured(true);
        }

        // Release capture when RMB is released (even if cursor left the panel while dragging)
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && g_input.IsMouseCaptured())
        {
            g_input.SetMouseCaptured(false);
            SDL_WarpMouseInWindow(g_SDLWindow, storedMouseX, storedMouseY);
        }
    }

    ImGui::End();

	// HIERARCHY WINDOW
    ImGui::Begin("Hierarchy");
    {
		// List all entities with a NameComponent in the hierarchy
        auto view = g_scene.registry.view<Engine::NameComponent>();
        for (auto entity : view)
        {
            auto& nameComp = view.get<Engine::NameComponent>(entity);

			// Set tree node flags: leaf because we don't have parent-child relationships yet, and span width for better clickability
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            // Note: Leaf because entities do not have children yet
            if (g_selectedEntity == entity)
                flags |= ImGuiTreeNodeFlags_Selected;

			// Use the entity ID as the ImGui tree node ID to ensure uniqueness
            bool opened = ImGui::TreeNodeEx((void*)(uint32_t)entity, flags, "%s", nameComp.name.c_str());
			// Handle selection: clicking on the item selects it
            if (ImGui::IsItemClicked()) { g_selectedEntity = entity; }

			// No child nodes for now since we don't have parent-child relationships, but if we did, they would go here
            if (opened) { ImGui::TreePop(); }
        }

        // Deselection: click empty space in the window to clear selection
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            g_selectedEntity = entt::null;
        }
    }
    ImGui::End();

	// INSPECTOR WINDOW
    ImGui::Begin("Inspector");
    {
        if (g_selectedEntity != entt::null && g_scene.registry.valid(g_selectedEntity))
        {
            // NameComponent UI
            if (g_scene.registry.all_of<Engine::NameComponent>(g_selectedEntity))
            {
                auto& nc = g_scene.registry.get<Engine::NameComponent>(g_selectedEntity);

                static char buffer[256] = {};
#ifdef _MSC_VER
                strncpy_s(buffer, nc.name.c_str(), sizeof(buffer) - 1);
#else
                std::strncpy(buffer, nc.name.c_str(), sizeof(buffer) - 1);
#endif

                if (ImGui::InputText("Name", buffer, sizeof(buffer)))
                {
                    nc.name = buffer;
                }
            }

            // TransformComponent UI
            if (g_scene.registry.all_of<Engine::TransformComponent>(g_selectedEntity))
            {
                auto& tc = g_scene.registry.get<Engine::TransformComponent>(g_selectedEntity);

                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat3("Position", &tc.position.x, 0.1f);
                    ImGui::DragFloat3("Scale", &tc.scale.x, 0.1f);

                    // Static cache to hold UI state between frames
                    static entt::entity s_lastEntity = entt::null;
                    static DirectX::XMFLOAT3 s_cachedEuler{ 0.0f, 0.0f, 0.0f };

                    // 1. Check if the selection changed
                    bool selectionChanged = (s_lastEntity != g_selectedEntity);
                    s_lastEntity = g_selectedEntity;

                    // 2. Check if the quaternion was changed externally (e.g., by Physics)
                    // Compare the actual quaternion against the one generated by cached Euler angles.
                    DirectX::XMFLOAT4 expectedQuat = Engine::Math::EulerDegreesToQuaternion(s_cachedEuler);
                    DirectX::XMVECTOR q1 = DirectX::XMLoadFloat4(&expectedQuat);
                    DirectX::XMVECTOR q2 = DirectX::XMLoadFloat4(&tc.rotation);

                    // Use dot product to check if quaternions are virtually identical
                    float dot = fabs(DirectX::XMVectorGetX(DirectX::XMQuaternionDot(q1, q2)));
                    bool externallyChanged = (dot < 0.9999f);

                    // 3. Update the cache ONLY if selection changed or physics moved the object
                    if (selectionChanged || externallyChanged)
                    {
                        s_cachedEuler = Engine::Math::QuaternionToEulerDegrees(tc.rotation);
                    }

                    // 4. Draw the UI using the stable cached values
                    if (ImGui::DragFloat3("Rotation", &s_cachedEuler.x, 1.0f))
                    {
                        // 5. If the user drags the slider, push the new rotation to the component
                        tc.rotation = Engine::Math::EulerDegreesToQuaternion(s_cachedEuler);
                    }
                }
            }

            // LightComponent UI
            if (g_scene.registry.all_of<Engine::LightComponent>(g_selectedEntity))
            {
                auto& lc = g_scene.registry.get<Engine::LightComponent>(g_selectedEntity);

                if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::ColorEdit3("Color", &lc.color.x);
                    ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 1000.0f);
                    ImGui::DragFloat("Range", &lc.range, 0.5f, 0.0f, 1000.0f);
                }
            }

            // RigidBodyComponent UI
            if (g_scene.registry.all_of<Engine::RigidBodyComponent>(g_selectedEntity))
            {
                auto& rb = g_scene.registry.get<Engine::RigidBodyComponent>(g_selectedEntity);

                if (ImGui::CollapsingHeader("RigidBody"))
                {
                    // Note: these values currently dictate the initial state of the Jolt body.
                    // Changing them at runtime won't update the Jolt body yet for now.
                    ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.0f, 10000.0f);
                    ImGui::DragFloat("Friction", &rb.friction, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Linear Damping", &rb.linearDamping, 0.01f, 0.0f, 100.0f);
                }
            }

			// MeshRendererComponent UI
            if (g_scene.registry.all_of<Engine::MeshRendererComponent>(g_selectedEntity))
            {
                auto& mr = g_scene.registry.get<Engine::MeshRendererComponent>(g_selectedEntity);
                if (ImGui::CollapsingHeader("Mesh Renderer"))
                {
                    ImGui::DragFloat("Roughness", &mr.roughness, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Metallic", &mr.metallic, 0.01f, 0.0f, 1.0f);
                }
            }
        }
        else
        {
            ImGui::Text("No entity selected");
        }
    }
    ImGui::End();

	// CONTENT BROWSER WINDOW
    ImGui::Begin("Content Browser");
    {
        // Back button: only when inside a subfolder (never go above assets root)
        if (g_currentDirectory != g_assetPath)
        {
            if (ImGui::Button("<- Back"))
            {
                g_currentDirectory = g_currentDirectory.parent_path();
            }
        }

        // Iterate and display the current directory
        for (auto& directoryEntry : std::filesystem::directory_iterator(g_currentDirectory))
        {
            const auto& path = directoryEntry.path();
            std::string filenameString = path.filename().string();

            // Directories: clickable to navigate into
            if (directoryEntry.is_directory())
            {
                if (ImGui::Selectable(("[DIR] " + filenameString).c_str()))
                {
                    g_currentDirectory /= path.filename();
                }
            }
            else
            {
                // Files: display only for now (drag/drop & open actions later)
                ImGui::Text("[FILE] %s", filenameString.c_str());
            }
        }
    }
    ImGui::End();

    // Render the 3D scene into the off-screen framebuffer (Render-to-Texture)
    g_renderer.BindFramebuffer();

    Engine::RenderSystem::DrawEntities(g_scene, g_meshManager, g_shaderManager, g_renderer, g_textureManager);

    // Draw skybox last: z=w ensures it renders only where nothing else drew
    if (g_scene.m_activeRenderCamera != entt::null &&
        g_scene.registry.valid(g_scene.m_activeRenderCamera) &&
        g_scene.registry.all_of<Engine::TransformComponent, Engine::CameraComponent>(g_scene.m_activeRenderCamera))
    {
        const auto& camTrans = g_scene.registry.get<Engine::TransformComponent>(g_scene.m_activeRenderCamera);
        const auto& camComp  = g_scene.registry.get<Engine::CameraComponent>(g_scene.m_activeRenderCamera);
        g_renderer.DrawSkybox(g_meshManager, g_shaderManager, camComp, camTrans);
    }

    // Now bind the real swapchain back buffer.
    // NOTE: The window will intentionally render black until ImGui displays the framebuffer SRV.
    g_renderer.BindBackBuffer();

    // Draw the UI data to the cleared backbuffer
    g_imGuiManager.EndFrame();

    g_renderer.Present(g_vSync);
}