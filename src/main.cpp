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

    // Create a directional light
    g_scene.CreateDirectionalLight("Sun Light");

  //  // Create a sample point light (red) near the model
  //  g_scene.CreatePointLight(
  //      "Red Point Light",
  //      XMFLOAT3{ 0.0f, 15.0f, 10.0f },     // position
		//XMFLOAT3{ 1.0f, 1.0f, 1.0f },       // color (red)
  //      100.0f,                              // intensity
  //      1000.0f                               // range
  //  );

  //  g_scene.CreatePointLight(
  //      "Red Point Light",
  //      XMFLOAT3{ 0.0f, 15.0f, 0.0f },     // position
  //      XMFLOAT3{ 1.0f, 1.0f, 1.0f },       // color (red)
  //      100.0f,                              // intensity
  //      1000.0f                               // range
  //  );

  //  g_scene.CreatePointLight(
  //      "Red Point Light",
  //      XMFLOAT3{ 0.0f, 15.0f, -10.0f },     // position
  //      XMFLOAT3{ 1.0f, 1.0f, 1.0f },       // color (red)
  //      100.0f,                              // intensity
  //      1000.0f                               // range
  //  );

  //  g_scene.CreatePointLight(
  //      "Red Point Light",
  //      XMFLOAT3{ 0.0f, 15.0f, -15.0f },     // position
  //      XMFLOAT3{ 1.0f, 1.0f, 1.0f },       // color (red)
  //      100.0f,                              // intensity
  //      1000.0f                               // range
  //  );

	// Create a sample spot light (blue) aimed at the model from above
  //  {
		//// calculate direction vector from position to target
  //      XMFLOAT3 spotPos{ 0.0f, -2.0f, 0.0f };
  //      XMFLOAT3 target{ 0.0f, -100.0f, 0.0f };
  //      XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&target), XMLoadFloat3(&spotPos)));
  //      XMFLOAT3 dirF{};
  //      XMStoreFloat3(&dirF, dir);

  //      g_scene.CreateSpotLight(
  //          "Blue Spot Light",
  //          spotPos,
  //          dirF,
		//	XMFLOAT3{ 0.2f, 0.4f, 1.0f },       // color (blue)
  //          100.0f,                             // intensity
  //          20.0f,                              // range
  //          XM_PIDIV4                           // 45 deg cone
  //      );
  //  }

    // Load a model; ensure the asset exists and Assimp DLL is present alongside the exe
    auto meshIDs = g_meshManager.LoadModel(g_renderer.GetDevice(), "assets/Models/MyModel3.obj");

    for (int id : meshIDs)
    {
        auto e = g_scene.CreateEntity("ModelPart");
        auto& t = g_scene.registry.get<Engine::TransformComponent>(e);
        t.scale = { 1.0f, 1.0f, 1.0f }; // Adjust scale if needed (0.01f or 100.0f)
        XMVECTOR qx = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XM_PIDIV2);
        XMStoreFloat4(&t.rotation, qx);

        auto& mr = g_scene.registry.emplace<Engine::MeshRendererComponent>(e);
        mr.meshID = id;
        mr.materialID = shaderID; // Basic Material
        mr.metallic = 0.0f; // FORCE NON-METALLIC (Fixes Black Texture)
        mr.roughness = 0.8f;

        ID3D11ShaderResourceView* tex = g_textureManager.LoadTexture(g_renderer.GetDevice(), "assets/Textures/MyTexture3.jpg");
        mr.texture = tex; // assign texture to component
    }

    //meshIDs = g_meshManager.LoadModel(g_renderer.GetDevice(), "assets/Models/MyModel.obj");
    //// Create the sample entity
    //{
    //    g_sampleEntity = g_scene.CreateSampleEntity("Rotating 3D Model");

    //    // Hook the sample entity to resources (now using first mesh from model)
    //    auto& mr = g_scene.registry.get<Engine::MeshRendererComponent>(g_sampleEntity);
    //    if (!meshIDs.empty())
    //    {
    //        int firstMeshID = meshIDs[0];
    //        mr.meshID = firstMeshID;
    //    }
    //    else {
    //        throw std::runtime_error("Failed to load model meshes.");
    //    }
    //    //else
    //    //{
    //    //    // fallback to temp cube if model failed to load
    //    //    mr.meshID = 101;    // per spec, temporary ID
    //    //}

    //    mr.materialID = shaderID;  // map materialID -> shaderID(1) (temporary ID)

    //    // example texture loading via texture manager and keep SRV
    //    ID3D11ShaderResourceView* tex = g_textureManager.LoadTexture(g_renderer.GetDevice(), "assets/Textures/MyTexture.png");
    //    mr.texture = tex; // assign texture to component
    //    // PBR value testing
    //    mr.roughness = 0.3f; // shiny
    //    mr.metallic = 0.2f; // metallic (with yellow-ish albedo you'd get gold-like)

    //    //Engine::RigidBodyComponent rb{};
    //    //rb.shape = Engine::RBShape::Mesh;
    //    //rb.motionType = Engine::RBMotion::Dynamic;
    //    //rb.mass = 1.0f;
    //    //rb.meshID = mr.meshID; // use same mesh for collider
    //    //g_scene.registry.emplace<Engine::RigidBodyComponent>(g_sampleEntity, rb);
    //}

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


	// PHYSICS TEST ENTITIES
    // Ground (static box)
  //  {
  //      entt::entity ground = g_scene.CreateEntity("Ground");
  //      auto& tc = g_scene.registry.get<Engine::TransformComponent>(ground);
  //      tc.position = XMFLOAT3(0.0f, -5.0f, 0.0f);
  //      tc.scale = XMFLOAT3(20.0f, 0.1f, 20.0f); // visual scaling to match collider

  //      Engine::RigidBodyComponent rb{};
  //      rb.shape = Engine::RBShape::Box;
  //      rb.motionType = Engine::RBMotion::Static;
  //      g_scene.registry.emplace<Engine::RigidBodyComponent>(ground, rb);

  //      Engine::MeshRendererComponent rend{};
  //      rend.meshID = 101;               // cube mesh
  //      rend.materialID = shaderID;
  //      rend.roughness = 0.1f;
  //      rend.metallic = 0.2f;
  //      g_scene.registry.emplace<Engine::MeshRendererComponent>(ground, rend);
  //  }

    // Falling Box (dynamic)
    /*{
        entt::entity box = g_scene.CreateEntity("Physics Box");
        auto& tc = g_scene.registry.get<Engine::TransformComponent>(box);
        tc.position = XMFLOAT3(1.0f, 20.0f, 2.0f);
        tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

        Engine::RigidBodyComponent rb{};
        rb.shape = Engine::RBShape::Box;
        rb.motionType = Engine::RBMotion::Static;
        rb.mass = 1.0f;
        g_scene.registry.emplace<Engine::RigidBodyComponent>(box, rb);

        Engine::MeshRendererComponent rend{};
        rend.meshID = 101;
        rend.materialID = shaderID;
        rend.roughness = 0.1f;
        rend.metallic = 0.2f;
        g_scene.registry.emplace<Engine::MeshRendererComponent>(box, rend);
    }*/

  //  // Falling Sphere (dynamic)
  //  {
  //      entt::entity sphere = g_scene.CreateEntity("Physics Sphere");
  //      auto& tc = g_scene.registry.get<Engine::TransformComponent>(sphere);
  //      tc.position = XMFLOAT3(0.5f, 20.0f, 2.0f);
  //      tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

  //      Engine::RigidBodyComponent rb{};
  //      rb.shape = Engine::RBShape::Sphere;
  //      rb.motionType = Engine::RBMotion::Dynamic;
  //      rb.mass = 1.0f;
  //      rb.radius = 0.5f;
		//rb.restitution = 0.5f; // bouncy
  //      g_scene.registry.emplace<Engine::RigidBodyComponent>(sphere, rb);

  //      Engine::MeshRendererComponent rend{};
  //      rend.meshID = g_meshManager.CreateSphere(g_renderer.GetDevice(), 0.5f, 32, 32); // radius matches physics
  //      rend.materialID = shaderID;
  //      rend.roughness = 0.1f;
  //      rend.metallic = 0.2f;
  //      g_scene.registry.emplace<Engine::MeshRendererComponent>(sphere, rend);
  //  }

  //  // Falling Capsule (dynamic)
  //  {
  //      entt::entity capsule = g_scene.CreateEntity("Physics Capsule");
  //      auto& tc = g_scene.registry.get<Engine::TransformComponent>(capsule);
  //      tc.position = XMFLOAT3(2.0f, 10.0f, 2.0f);
  //      tc.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

  //      Engine::RigidBodyComponent rb{};
  //      rb.shape = Engine::RBShape::Capsule;
  //      rb.motionType = Engine::RBMotion::Dynamic;
  //      rb.mass = 1.0f;
  //      rb.radius = 0.5f;
  //      rb.height = 1.0f; // cylinder height in PhysicsManager logic
  //      g_scene.registry.emplace<Engine::RigidBodyComponent>(capsule, rb);

  //      Engine::MeshRendererComponent rend{};
  //      rend.meshID = g_meshManager.CreateCapsule(g_renderer.GetDevice(), 0.5f, 1.0f, 32, 32);
  //      rend.materialID = shaderID;
  //      rend.roughness = 0.1f;
  //      rend.metallic = 0.2f;
  //      g_scene.registry.emplace<Engine::MeshRendererComponent>(capsule, rend);
  //  }
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

    Engine::CameraInputSystem(g_scene, g_input, deltaTime);
    Engine::CameraMatrixSystem(g_scene, g_renderer);
    //Engine::DemoRotationSystem(g_scene, g_sampleEntity, deltaTime);

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