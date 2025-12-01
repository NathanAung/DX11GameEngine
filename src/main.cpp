#include <cstdio>       // For std::fprintf
#include <stdexcept>    // For std::runtime_error
#include <string>       // For std::string
#include <vector>       // For std::vector

// SDL2
#include <SDL.h>
#include <SDL_syswm.h>

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>     // For Microsoft::WRL::ComPtr
#include <DirectXMath.h>

// Link necessary libraries
// pragma comment is a Microsoft-specific directive to link libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

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



// Main entry point
int main(int argc, char** argv)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    const int windowWidth = 1280;
    const int windowHeight = 720;

    // Create SDL window suitable for DirectX
    SDL_Window* window = SDL_CreateWindow(
        "DX11GameEngine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Get HWND from SDL window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo))
    {
        std::fprintf(stderr, "SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    HWND hwnd = wmInfo.info.win.window;

    // Initialize DirectX 11
    DX11Context dx;
    try { 
        InitD3D11(hwnd, dx, (UINT)windowWidth, (UINT)windowHeight); 
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "DirectX init failed: %s\n", e.what());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Shaders
    ComPtr<ID3DBlob> vsBytecode = CompileShader(L"shaders/BasicVS.hlsl", "main", "vs_5_0");
    ComPtr<ID3DBlob> psBytecode = CompileShader(L"shaders/BasicPS.hlsl", "main", "ps_5_0");

	// Create shader objects
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    HRESULT hr = dx.device->CreateVertexShader(vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(), nullptr, vertexShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed");
    hr = dx.device->CreatePixelShader(psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(), nullptr, pixelShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed");

	// Input layout for vertex structure
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    ComPtr<ID3D11InputLayout> inputLayout;
    hr = dx.device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc),
        vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(),
        inputLayout.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed");

    // Cube geometry
    const float s = 0.5f;
    std::vector<Vertex> vertices = {
        {{-s,-s,-s},{0,0,0}}, {{-s,+s,-s},{0,1,0}}, {{+s,+s,-s},{1,1,0}}, {{+s,-s,-s},{1,0,0}}, // back
        {{-s,-s,+s},{0,0,1}}, {{-s,+s,+s},{0,1,1}}, {{+s,+s,+s},{1,1,1}}, {{+s,-s,+s},{1,0,1}}  // front
    };
    std::vector<uint16_t> indices = {
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
    vbDesc.ByteWidth = UINT(vertices.size() * sizeof(Vertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();
    ComPtr<ID3D11Buffer> vertexBuffer;
    hr = dx.device->CreateBuffer(&vbDesc, &vbData, vertexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer(VB) failed");

	// index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = UINT(indices.size() * sizeof(uint16_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();
    ComPtr<ID3D11Buffer> indexBuffer;
    hr = dx.device->CreateBuffer(&ibDesc, &ibData, indexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer(IB) failed");

    // Rasterizer state
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_BACK;      // don't draw back faces
	rsDesc.FrontCounterClockwise = FALSE;   // clockwise vertices are front-facing
	rsDesc.DepthClipEnable = TRUE;          // enable depth clipping
    ComPtr<ID3D11RasterizerState> rasterState;
    hr = dx.device->CreateRasterizerState(&rsDesc, rasterState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRasterizerState failed");

	// Depth-stencil state
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // enable depth writes
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;           // standard less-than depth test
    dsDesc.StencilEnable = FALSE;
    ComPtr<ID3D11DepthStencilState> depthStencilState;
    hr = dx.device->CreateDepthStencilState(&dsDesc, depthStencilState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilState failed");

    // Constant buffers: Projection(b0), View(b1), World(b2)
    ComPtr<ID3D11Buffer> cbProjection = CreateMatrixCB(dx.device.Get());
    ComPtr<ID3D11Buffer> cbView = CreateMatrixCB(dx.device.Get());
    ComPtr<ID3D11Buffer> cbWorld = CreateMatrixCB(dx.device.Get());

	// Initial projection matrix
    auto UpdateProjectionForSize = [&](UINT w, UINT h)
        {
			//aspect ratio
            float aspect = (h == 0) ? 1.0f : (float)w / (float)h;
            XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
            UpdateMatrixCB(dx.context.Get(), cbProjection.Get(), proj);
        };
    UpdateProjectionForSize(dx.width, dx.height);

	// Main loop
    bool running = true;
    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 lastCounter = SDL_GetPerformanceCounter();
    float angle = 0.0f;

    while (running)
    {
		// Event handling
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                try
                {
                    Resize(dx, (UINT)e.window.data1, (UINT)e.window.data2);
                    UpdateProjectionForSize(dx.width, dx.height);
                }
                catch (const std::exception& ex)
                {
                    std::fprintf(stderr, "Resize error: %s\n", ex.what());
                }
            }
        }

		// Update
        Uint64 currentCounter = SDL_GetPerformanceCounter();
		double dt = double(currentCounter - lastCounter) / double(perfFreq);    // delta time in seconds
        lastCounter = currentCounter;

        angle += float(dt) * XM_PIDIV4;

        // View and World (row-major upload)
		XMVECTOR eye = XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);    // camera position
		XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);      // look-at target
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);      // up direction
		XMMATRIX view = XMMatrixLookAtLH(eye, at, up);          // view matrix
		XMMATRIX world = XMMatrixRotationX(angle) * XMMatrixRotationY(angle * 0.7f);    // world matrix, rotating cube

		// Update constant buffers
        UpdateMatrixCB(dx.context.Get(), cbView.Get(), view);
        UpdateMatrixCB(dx.context.Get(), cbWorld.Get(), world);

        // Viewport
        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
        vp.Width = float(dx.width); vp.Height = float(dx.height);
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        dx.context->RSSetViewports(1, &vp);

		// OM; output merger stage which binds render targets and depth-stencil
        dx.context->OMSetRenderTargets(1, dx.rtv.GetAddressOf(), dx.dsv.Get());

		// Clear render target and depth-stencil
        const float clearColor[4] = { 0.10f, 0.18f, 0.28f, 1.0f };
        dx.context->ClearRenderTargetView(dx.rtv.Get(), clearColor);
        dx.context->ClearDepthStencilView(dx.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// IA; input assembler stage which binds vertex/index buffers and input layout
        dx.context->IASetInputLayout(inputLayout.Get());
        UINT stride = sizeof(Vertex), offset = 0;
        dx.context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        dx.context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        dx.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// RS and DS states
        dx.context->RSSetState(rasterState.Get());
        dx.context->OMSetDepthStencilState(depthStencilState.Get(), 0);

        // Shaders + CBs (b0=Proj, b1=View, b2=World)
        dx.context->VSSetShader(vertexShader.Get(), nullptr, 0);
        ID3D11Buffer* vscbs[] = { cbProjection.Get(), cbView.Get(), cbWorld.Get() };
        dx.context->VSSetConstantBuffers(0, 3, vscbs);
        dx.context->PSSetShader(pixelShader.Get(), nullptr, 0);

		// Draw call
        dx.context->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);
        dx.swapChain->Present(1, 0);
    }

    // Cleanup
    cbWorld.Reset(); cbView.Reset(); cbProjection.Reset();
    indexBuffer.Reset(); vertexBuffer.Reset();
    inputLayout.Reset(); vertexShader.Reset(); pixelShader.Reset();
    dx.dsv.Reset(); dx.depthStencilBuffer.Reset(); dx.rtv.Reset();
    dx.swapChain.Reset(); dx.context.Reset(); dx.device.Reset();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}