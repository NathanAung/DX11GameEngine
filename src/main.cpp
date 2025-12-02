#include "Engine/Core.h"
#include "Engine/InputManager.h"
#include "Engine/Camera.h"
// Common Usings
using Microsoft::WRL::ComPtr;   // template smart pointer for COM objects
using namespace DirectX;


// Structure to hold DirectX 11 context
struct DX11Context
{
    ComPtr<ID3D11Device> device;                                // used to create resources
    ComPtr<ID3D11DeviceContext> context;                        // used for rendering commands
    ComPtr<IDXGISwapChain> swapChain;                           // swap chain for presenting frames
    ComPtr<ID3D11RenderTargetView> rtv;                         // render target view for the back buffer
	ComPtr<ID3D11Texture2D> depthStencilBuffer;                 // depth-stencil buffer for depth testing
	ComPtr<ID3D11DepthStencilView> dsv;                         // depth-stencil view for binding depth buffer
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;    // feature level of the device
    UINT width = 1280;                                          // default width
    UINT height = 720;                                          // default height
};

// Vertex structure
struct Vertex { XMFLOAT3 position; XMFLOAT3 color; };

// GLOBAL VARIABLES (made global to allow Update / Render / Clear / Present etc. to keep main clean)
SDL_Window* g_SDLWindow = nullptr;
HWND g_Hwnd = nullptr;
DX11Context g_dx;

// Window dimensions
const int g_windowWidth = 1280;
const int g_windowHeight = 720;

// Shaders
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;

ComPtr<ID3D11InputLayout> g_inputLayout;    // input layout for vertex structure

// Buffers
ComPtr<ID3D11Buffer> g_vertexBuffer;
ComPtr<ID3D11Buffer> g_indexBuffer;

// States
ComPtr<ID3D11RasterizerState> g_rasterState;
ComPtr<ID3D11DepthStencilState> g_depthStencilState;

// Constant buffers
ComPtr<ID3D11Buffer> g_cbProjection;
ComPtr<ID3D11Buffer> g_cbView;
ComPtr<ID3D11Buffer> g_cbWorld;

// Geometry data for a cube
std::vector<Vertex> g_vertices;
std::vector<uint16_t> g_indices;
float g_angle = 0.0f;

// Timing variables
Uint64 g_perfFreq = 0;
Uint64 g_lastCounter = 0;
bool g_running = true;
bool g_vSync = true; // can toggle later

// Input and Camera
Engine::InputManager g_input;
Engine::Camera g_camera;

// Forward declarations for helpers
static void CreateViews(DX11Context& dx);
static void Resize(DX11Context& dx, UINT newWidth, UINT newHeight);
static void InitD3D11(HWND hwnd, DX11Context& dx, UINT width, UINT height);
static ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const std::string& entry, const std::string& target);
static ComPtr<ID3D11Buffer> CreateMatrixCB(ID3D11Device* dev);
static void UpdateMatrixCB(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const XMMATRIX& m);
void UnloadContent();
void Update(float deltaTime);
void Render();
void Cleanup();



// Function to create render target view
static void CreateViews(DX11Context& dx)
{
    // Release existing RTV if any
    dx.rtv.Reset();
    dx.depthStencilBuffer.Reset();
    dx.dsv.Reset();

    // back buffer texture which will be used to create the RTV
    ComPtr<ID3D11Texture2D> backBuffer;
    // Get the back buffer from the swap chain
    HRESULT hr = dx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) throw std::runtime_error("GetBuffer(backBuffer) failed");

    // Create the render target view from the back buffer
    hr = dx.device->CreateRenderTargetView(backBuffer.Get(), nullptr, dx.rtv.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRenderTargetView failed");

	// Create depth-stencil buffer and view
    D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = dx.width;                         // Match the swap chain dimensions
	depthDesc.Height = dx.height;                       
	depthDesc.MipLevels = 1;                            // No mipmaps
	depthDesc.ArraySize = 1;                            // Single texture
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;   // 24 bits for depth, 8 bits for stencil
	depthDesc.SampleDesc.Count = 1;                     // No multisampling
    depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;              // GPU read/write
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;     // Bind as depth-stencil buffer

	// Create the depth-stencil texture
    hr = dx.device->CreateTexture2D(&depthDesc, nullptr, dx.depthStencilBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateTexture2D(depth) failed");

	// Create the depth-stencil view
    hr = dx.device->CreateDepthStencilView(dx.depthStencilBuffer.Get(), nullptr, dx.dsv.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilView failed");
}



// Function to handle window resizing
static void Resize(DX11Context& dx, UINT newWidth, UINT newHeight)
{
    if (newWidth == 0 || newHeight == 0) return;    // Minimized
    dx.width = newWidth;
    dx.height = newHeight;

	// Unbind and release current views
    dx.context->OMSetRenderTargets(0, nullptr, nullptr);
    dx.rtv.Reset();
    dx.depthStencilBuffer.Reset();
    dx.dsv.Reset();

    // Resize the swap chain buffers
    HRESULT hr = dx.swapChain->ResizeBuffers(0, dx.width, dx.height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) throw std::runtime_error("ResizeBuffers failed");

	// Recreate render target and depth-stencil views
    CreateViews(dx);
}



// Function to initialize DirectX 11
static void InitD3D11(HWND hwnd, DX11Context& dx, UINT width, UINT height)
{
    dx.width = width;
    dx.height = height;

    // Device creation flags, enable debug layer in debug builds
    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Feature levels to attempt to create
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    // Describe the swap chain
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;                                     // Double buffering, better performance
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;      // 32-bit color format (8 bits for red, green, blue, and alpha channels)
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;       // Use the back buffer as a render target
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;                                // number of samples per pixel, no multi-sampling
    sd.SampleDesc.Count = 1;                                
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    // SwapEffect specifies how the swap chain should handle presenting the back buffer to the front buffer
    // DXGI_SWAP_EFFECT_DISCARD: the contents of the back buffer are discarded after presenting
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Create the device, device context, and swap chain
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // Use default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Hardware driver
        nullptr,                    // No software rasterizer
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &sd,
        swapChain.GetAddressOf(),
        device.GetAddressOf(),
        &dx.featureLevel,
        context.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("D3D11CreateDeviceAndSwapChain failed");

    dx.device = device;
    dx.context = context;
    dx.swapChain = swapChain;

    // Create the render target view for the back buffer
    CreateViews(dx);
}



// Function to compile a shader from file
static ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const std::string& entry, const std::string& target)
{
	// Compile flags, enable strictness and row-major packing
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;

    HRESULT hr = D3DCompileFromFile(
        path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry.c_str(),
        target.c_str(),
        flags,
        0,
        bytecode.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::fprintf(stderr, "Shader compile error (%ls): %s\n", path.c_str(), (char*)errors->GetBufferPointer());
        throw std::runtime_error("Failed to compile shader");
    }
    return bytecode;
}



// Creates a constant buffer for a 4x4 matrix
// Constant buffers: Projection (b0), View (b1), World (b2)
static ComPtr<ID3D11Buffer> CreateMatrixCB(ID3D11Device* dev)
{
    D3D11_BUFFER_DESC cb = {};
	cb.Usage = D3D11_USAGE_DEFAULT;             // GPU read/write
	cb.ByteWidth = sizeof(XMFLOAT4X4);          // size of a 4x4 matrix
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;  // bind as constant buffer
	cb.CPUAccessFlags = 0;                      // no CPU access needed

    ComPtr<ID3D11Buffer> buf;
    HRESULT hr = dev->CreateBuffer(&cb, nullptr, buf.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer(CB) failed");
    return buf;
}



// Updates a matrix constant buffer with the given matrix
static void UpdateMatrixCB(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const XMMATRIX& m)
{
    XMFLOAT4X4 mOut;
    XMStoreFloat4x4(&mOut, m); // row-major store, matches HLSL row_major and compile flag
	ctx->UpdateSubresource(cb, 0, nullptr, &mOut, 0, 0);    // update the constant buffer
}



// Helper to initialize all GPU content (was previously inline in main)
static void LoadContent()
{
    // Shaders
    ComPtr<ID3DBlob> vsBytecode = CompileShader(L"shaders/BasicVS.hlsl", "main", "vs_5_0");
    ComPtr<ID3DBlob> psBytecode = CompileShader(L"shaders/BasicPS.hlsl", "main", "ps_5_0");

	// Create shader objects
    HRESULT hr = g_dx.device->CreateVertexShader(vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(), nullptr, g_vertexShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed");
    hr = g_dx.device->CreatePixelShader(psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(), nullptr, g_pixelShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed");

	// Input layout for vertex structure
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = g_dx.device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc),
        vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(),
        g_inputLayout.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed");

    // Cube geometry
    const float s = 0.5f;
    g_vertices = {
        {{-s,-s,-s},{0,0,0}}, {{-s,+s,-s},{0,1,0}}, {{+s,+s,-s},{1,1,0}}, {{+s,-s,-s},{1,0,0}}, // back
        {{-s,-s,+s},{0,0,1}}, {{-s,+s,+s},{0,1,1}}, {{+s,+s,+s},{1,1,1}}, {{+s,-s,+s},{1,0,1}}  // front
    };
    g_indices = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        4, 5, 1, 4, 1, 0,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7
    };

    // Buffers
	// vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = UINT(g_vertices.size() * sizeof(Vertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = g_vertices.data();
    hr = g_dx.device->CreateBuffer(&vbDesc, &vbData, g_vertexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer(VB) failed");

	// index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = UINT(g_indices.size() * sizeof(uint16_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = g_indices.data();
    hr = g_dx.device->CreateBuffer(&ibDesc, &ibData, g_indexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer(IB) failed");

    // Rasterizer state
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_BACK;      // don't draw back faces
	rsDesc.FrontCounterClockwise = FALSE;   // clockwise vertices are front-facing
	rsDesc.DepthClipEnable = TRUE;          // enable depth clipping
    hr = g_dx.device->CreateRasterizerState(&rsDesc, g_rasterState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRasterizerState failed");

	// Depth-stencil state
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // enable depth writes
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;           // standard less-than depth test
    dsDesc.StencilEnable = FALSE;
    hr = g_dx.device->CreateDepthStencilState(&dsDesc, g_depthStencilState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilState failed");

    // Constant buffers: Projection(b0), View(b1), World(b2)
    g_cbProjection = CreateMatrixCB(g_dx.device.Get());
    g_cbView       = CreateMatrixCB(g_dx.device.Get());
    g_cbWorld      = CreateMatrixCB(g_dx.device.Get());

	// Initial matrices from camera
    UpdateMatrixCB(g_dx.context.Get(), g_cbProjection.Get(), g_camera.GetProjectionMatrix());
    UpdateMatrixCB(g_dx.context.Get(), g_cbView.Get(), g_camera.GetViewMatrix());
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

    // Initialize DirectX 11
    try { 
        InitD3D11(g_Hwnd, g_dx, (UINT)g_windowWidth, (UINT)g_windowHeight); 
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "DirectX init failed: %s\n", e.what());
        SDL_DestroyWindow(g_SDLWindow);
        SDL_Quit();
        return -1;
    }

    // Initialize camera and capture mouse
    g_camera.SetViewport(g_dx.width, g_dx.height);
    g_camera.SetLens(XM_PIDIV4, 0.1f, 100.0f);
    g_camera.SetPosition(XMFLOAT3{ 0.0f, 0.0f, -5.0f });
    g_input.SetMouseCaptured(true);

    // Load GPU content (shaders, buffers, states, constant buffers, geometry)
    try {
        LoadContent();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Content load failed: %s\n", e.what());
        Cleanup();
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
                    Resize(g_dx, (UINT)e.window.data1, (UINT)e.window.data2);
                    g_camera.SetViewport(g_dx.width, g_dx.height);
                    UpdateMatrixCB(g_dx.context.Get(), g_cbProjection.Get(), g_camera.GetProjectionMatrix());
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

        // Render geometry & present
        Render();
    }

    // Cleanup
    UnloadContent();
    Cleanup();
    return 0;
}



// UPDATE SCENE
// set up the camera view matrix and update the world matrix to rotate the cube over time
void Update(float deltaTime) {
    // Camera driven by InputManager
    g_camera.UpdateFromInput(g_input, deltaTime);

    // World rotation for the cube
    g_angle += deltaTime * XM_PIDIV4;
	XMMATRIX world = XMMatrixRotationX(g_angle) * XMMatrixRotationY(g_angle * 0.7f);

	// Update constant buffers (View and World). Projection is updated on resize/init.
    UpdateMatrixCB(g_dx.context.Get(), g_cbView.Get(), g_camera.GetViewMatrix());
    UpdateMatrixCB(g_dx.context.Get(), g_cbWorld.Get(), world);
}



// HELPER FUNCTION TO CLEAR THE RENDER TARGET AND DEPTH/ STENCIL BUFFER
// Clear the color and depth buffers.
void Clear(const FLOAT clearColor[4], FLOAT clearDepth, UINT8 clearStencil)
{
    // OM; output merger stage which binds render targets and depth-stencil
    g_dx.context->OMSetRenderTargets(1, g_dx.rtv.GetAddressOf(), g_dx.dsv.Get());
    g_dx.context->ClearRenderTargetView(g_dx.rtv.Get(), clearColor);
    g_dx.context->ClearDepthStencilView(g_dx.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clearDepth, clearStencil);
}



// HELPER FUNCTION TO PRESENT THE BACK BUFFER TO THE SCREEN
void Present(bool vSync)
{
    // Present method presents the back buffer to the front buffer (screen)
    // first parameter specifies the sync interval
    // second parameter specifies presentation options
    g_dx.swapChain->Present(vSync ? 1 : 0, 0);
}



// RENDER THE GEOMETRY
void Render()
{
    // Clear render target and depth-stencil
    const float clearColor[4] = { 0.10f, 0.18f, 0.28f, 1.0f };
    Clear(clearColor, 1.0f, 0);

    // Viewport
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
    vp.Width = float(g_dx.width); vp.Height = float(g_dx.height);
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    g_dx.context->RSSetViewports(1, &vp);

	// IA; input assembler stage which binds vertex/index buffers and input layout
    g_dx.context->IASetInputLayout(g_inputLayout.Get());
    UINT stride = sizeof(Vertex), offset = 0;
    g_dx.context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);
    g_dx.context->IASetIndexBuffer(g_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    g_dx.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// RS and DS states
    g_dx.context->RSSetState(g_rasterState.Get());
    g_dx.context->OMSetDepthStencilState(g_depthStencilState.Get(), 0);

    // Shaders + CBs (b0=Proj, b1=View, b2=World)
    g_dx.context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    ID3D11Buffer* vscbs[] = { g_cbProjection.Get(), g_cbView.Get(), g_cbWorld.Get() };
    g_dx.context->VSSetConstantBuffers(0, 3, vscbs);
    g_dx.context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

	// Draw call
    g_dx.context->DrawIndexed(static_cast<UINT>(g_indices.size()), 0, 0);

    Present(g_vSync);
}



// CLEANUP

// Release all game-specific assets
void UnloadContent()
{
    g_cbWorld.Reset(); 
    g_cbView.Reset(); 
    g_cbProjection.Reset();
    g_indexBuffer.Reset();
    g_vertexBuffer.Reset();
    g_inputLayout.Reset();
    g_vertexShader.Reset();
    g_pixelShader.Reset();
}

// Release all core system resources
void Cleanup()
{
    g_dx.dsv.Reset();
    g_dx.depthStencilBuffer.Reset();
    g_dx.rtv.Reset();
    g_dx.swapChain.Reset();
    g_dx.context.Reset();
    g_dx.device.Reset();

    if (g_SDLWindow) {
        SDL_DestroyWindow(g_SDLWindow);
        g_SDLWindow = nullptr;
    }
    SDL_Quit();
}