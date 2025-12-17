#include "Engine/Core.h"
#include "Engine/Renderer.h"
#include "Engine/InputManager.h"
#include "Engine/Scene.h"
#include "Engine/MeshManager.h"
#include "Engine/ShaderManager.h"
#include "Engine/Systems.h"
#include "Engine/TextureManager.h"

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

// ECS: Scene and a sample 3d entity
Engine::Scene g_scene;
entt::entity g_sampleEntity = entt::null;

// Managers
Engine::MeshManager g_meshManager;
Engine::ShaderManager g_shaderManager;
Engine::TextureManager g_textureManager; // global texture manager instance

// Renderer
Engine::Renderer g_renderer;

// New: Physics
Engine::PhysicsManager g_physicsManager;

// Forward declarations
static void LoadContent();
void Update(float deltaTime);
void Render();

static void LoadContent()
{
    // Create resources with renderer device
    const int shaderID   = g_shaderManager.LoadBasicShaders(g_renderer.GetDevice());

    // Ensure skybox cube mesh (ID 101) always exists for DrawSkybox
    // Note: keep this unconditional to guarantee Mesh 101 availability
    const int cubeMeshID = g_meshManager.InitializeCube(g_renderer.GetDevice()); // temporary ID 101

    // Compile & load skybox shaders (assign temporary ID 2 inside ShaderManager implementation)
    const int skyboxShaderID = g_shaderManager.LoadSkyboxShaders(g_renderer.GetDevice());

    // Create the editor camera entity
    g_scene.CreateEditorCamera("Main Editor Camera", g_renderer.GetWidth(), g_renderer.GetHeight());

    // Set initial camera position/look for mini-game
    if (g_scene.m_activeRenderCamera != entt::null &&
        g_scene.registry.valid(g_scene.m_activeRenderCamera) &&
        g_scene.registry.all_of<Engine::TransformComponent>(g_scene.m_activeRenderCamera))
    {
        auto& camTf = g_scene.registry.get<Engine::TransformComponent>(g_scene.m_activeRenderCamera);
        camTf.position = XMFLOAT3(0.0f, 5.0f, -15.0f);
        camTf.rotation = XMFLOAT4(0,0,0,1);
    }

    // Create a directional light
    g_scene.CreateDirectionalLight("Sun Light");

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
			std::printf("Skybox cubemap loaded successfully.\n");
        }
        else {
			throw std::runtime_error("Failed to load skybox cubemap textures.");
        }
    }

    // Ground (static, large)
    {
        entt::entity ground = g_scene.CreateEntity("Ground");
        auto& tc = g_scene.registry.get<Engine::TransformComponent>(ground);
        tc.position = XMFLOAT3(0.0f, -2.0f, 0.0f);
        tc.scale    = XMFLOAT3(50.0f, 1.0f, 50.0f); // visual scaling to match large plane

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Box;
        rb.motionType = Engine::RBMotion::Static;
        g_scene.registry.emplace<Engine::RigidBodyComponent>(ground, rb);

        Engine::MeshRendererComponent rend{};
        rend.meshID = 101;               // cube mesh (scaled big)
        rend.materialID = shaderID;
        rend.roughness = 0.8f;
        rend.metallic  = 0.8f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(ground, rend);
    }

    // spawn a wall of dynamic boxes initially
    // (Replicate SpawnWall helper locally to avoid coupling)
    {
        for (int y = 0; y <= 10; ++y)
        {
            for (int x = -5; x <= 5; ++x)
            {
                entt::entity e = g_scene.CreateEntity("Wall Block");
                auto& tc = g_scene.registry.get<Engine::TransformComponent>(e);
                tc.position = XMFLOAT3(x * 1.1f, 0.5f + y * 1.1f, 10.0f);
                tc.scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);

                Engine::MeshRendererComponent mr{};
                mr.meshID = 101;
                mr.materialID = shaderID;
                g_scene.registry.emplace<Engine::MeshRendererComponent>(e, mr);

                Engine::RigidBodyComponent rb{};
                rb.shape = Engine::RBShape::Box;
                rb.motionType = Engine::RBMotion::Dynamic;
                rb.halfExtent = XMFLOAT3(0.5f, 0.5f, 0.5f);
                auto& rbRef = g_scene.registry.emplace<Engine::RigidBodyComponent>(e, rb);

                // Create body immediately
                JPH::BodyID id = g_physicsManager.CreateRigidBody(tc, rbRef, g_meshManager);
                rbRef.bodyID = id;
                rbRef.bodyCreated = !id.IsInvalid();
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

    // Initialize physics (Jolt)
    if (!g_physicsManager.Initialize())
    {
        std::fprintf(stderr, "PhysicsManager initialization failed\n");
        g_renderer.Shutdown();
        if (g_SDLWindow) {
            SDL_DestroyWindow(g_SDLWindow);
            g_SDLWindow = nullptr;
        }
        SDL_Quit();
        return -1;
    }

    g_input.SetMouseCaptured(true);

    try {
        LoadContent();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Content load failed: %s\n", e.what());
        g_physicsManager.Shutdown();
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
            // Feed the input manager first (collect keyboard/mouse state)
            g_input.ProcessEvent(e);

            if (e.type == SDL_QUIT) g_running = false;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                try
                {
                    if (g_renderer.Resize((UINT)e.window.data1, (UINT)e.window.data2))
                    {
                        // update viewport component on active camera (optional)
                        if (g_scene.m_activeRenderCamera != entt::null &&
                            g_scene.registry.valid(g_scene.m_activeRenderCamera) &&
                            g_scene.registry.all_of<Engine::ViewportComponent>(g_scene.m_activeRenderCamera))
                        {
                            auto &vp = g_scene.registry.get<Engine::ViewportComponent>(g_scene.m_activeRenderCamera);
                            vp.width  = g_renderer.GetWidth();
                            vp.height = g_renderer.GetHeight();
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    std::fprintf(stderr, "Resize error: %s\n", ex.what());
                }
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

    // Mini-game system (spawn/reset/shoot + camera control)
    Engine::WallSmasherSystem(g_scene, g_physicsManager, g_meshManager, g_input, g_renderer, deltaTime);

    // Build camera matrices
    Engine::CameraMatrixSystem(g_scene, g_renderer);

    // exit on escape key
    if(g_input.IsKeyDown(Engine::Key::Esc))
    {
		g_running = false;
	}
}

void Render()
{
    g_renderer.BeginFrame();

    Engine::RenderSystem::DrawEntities(g_scene, g_meshManager, g_shaderManager, g_renderer);

    // Draw skybox last: z=w ensures it renders only where nothing else drew
    if (g_scene.m_activeRenderCamera != entt::null &&
        g_scene.registry.valid(g_scene.m_activeRenderCamera) &&
        g_scene.registry.all_of<Engine::TransformComponent, Engine::CameraComponent>(g_scene.m_activeRenderCamera))
    {
        const auto& camTrans = g_scene.registry.get<Engine::TransformComponent>(g_scene.m_activeRenderCamera);
        const auto& camComp  = g_scene.registry.get<Engine::CameraComponent>(g_scene.m_activeRenderCamera);
        g_renderer.DrawSkybox(g_meshManager, g_shaderManager, camComp, camTrans);
    }

    g_renderer.Present(g_vSync);
}