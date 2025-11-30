#include <cstdio>       // For std::fprintf
#include <stdexcept>    // For std::runtime_error
#include <string>       // For std::string

// SDL2
#include <SDL.h>
#include <SDL_syswm.h>

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h> // For Microsoft::WRL::ComPtr

// Link necessary libraries
// pragma comment is a Microsoft-specific directive to link libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// template smart pointer for COM objects
using Microsoft::WRL::ComPtr;



// Structure to hold DirectX 11 context
struct DX11Context
{
	ComPtr<ID3D11Device> device;                                // used to create resources
	ComPtr<ID3D11DeviceContext> context;                        // used for rendering commands
	ComPtr<IDXGISwapChain> swapChain;                           // swap chain for presenting frames
	ComPtr<ID3D11RenderTargetView> rtv;                         // render target view for the back buffer
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;    // feature level of the device
	UINT width = 1280;                                          // default width
	UINT height = 720;                                          // default height
};



// Function to create render target view
static void CreateRenderTarget(DX11Context& dx)
{
	// Release existing RTV if any
    dx.rtv.Reset();

	// back buffer texture which will be used to create the RTV
    ComPtr<ID3D11Texture2D> backBuffer;

	// Get the back buffer from the swap chain
    HRESULT hr = dx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) throw std::runtime_error("Failed to get swap chain back buffer");

	// Create the render target view from the back buffer
    hr = dx.device->CreateRenderTargetView(backBuffer.Get(), nullptr, dx.rtv.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Failed to create render target view");
}



// Function to handle window resizing
static void Resize(DX11Context& dx, UINT newWidth, UINT newHeight)
{
    if (newWidth == 0 || newHeight == 0) return; // Minimized
    dx.width = newWidth;
    dx.height = newHeight;

    // Unbind and release current RT
    ID3D11RenderTargetView* nullRTV = nullptr;
    dx.context->OMSetRenderTargets(1, &nullRTV, nullptr);
    dx.rtv.Reset();

	// Resize the swap chain buffers
    HRESULT hr = dx.swapChain->ResizeBuffers(0, dx.width, dx.height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) throw std::runtime_error("Failed to resize swap chain buffers");

	// Recreate the render target view for the new back buffer
    CreateRenderTarget(dx);
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
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

	// Describe the swap chain
    DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 2;                                 // Double buffering, better performance
	sd.BufferDesc.Width = width;                        
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // 32-bit color format (8 bits for red, green, blue, and alpha channels)
	sd.BufferDesc.RefreshRate.Numerator = 0;            // Let DXGI choose the refresh rate
    sd.BufferDesc.RefreshRate.Denominator = 0;          
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;   // Use the back buffer as a render target
    sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;                            // number of samples per pixel, no multi-sampling
	sd.SampleDesc.Quality = 0;                          // quality level of multi-sampling, 0 is the lowest quality
    sd.Windowed = TRUE;
    // SwapEffect specifies how the swap chain should handle presenting the back buffer to the front buffer
    // DXGI_SWAP_EFFECT_DISCARD: the contents of the back buffer are discarded after presenting
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Create the device, device context, and swap chain
    ComPtr<ID3D11Device> device;              
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                   // Use default adapter
        D3D_DRIVER_TYPE_HARDWARE,  // Hardware driver
        nullptr,                   // No software rasterizer
        createDeviceFlags,         
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &sd,
        swapChain.GetAddressOf(),
        device.GetAddressOf(),
        &dx.featureLevel,
        context.GetAddressOf()
    );
    if (FAILED(hr)) throw std::runtime_error("Failed to create D3D11 device and swap chain");

    dx.device = device;
    dx.context = context;
    dx.swapChain = swapChain;

	// Create the render target view for the back buffer
    CreateRenderTarget(dx);
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
    try
    {
        InitD3D11(hwnd, dx, static_cast<UINT>(windowWidth), static_cast<UINT>(windowHeight));
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "DirectX init failed: %s\n", e.what());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Main loop
    bool running = true;
    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (running)
    {
        // Input
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                running = false;
            }
            else if (e.type == SDL_WINDOWEVENT)
            {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    try
                    {
                        Resize(dx, static_cast<UINT>(e.window.data1), static_cast<UINT>(e.window.data2));
                    }
                    catch (const std::exception& ex)
                    {
                        std::fprintf(stderr, "Resize error: %s\n", ex.what());
                    }
                }
            }
            // Additional input handling in later steps
        }

        // Update
        Uint64 currentCounter = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(currentCounter - lastCounter) / static_cast<double>(perfFreq);
        lastCounter = currentCounter;
        (void)dt; // Placeholder; we’ll use dt in future steps

        // Render
        const float clearColor[4] = { 0.10f, 0.18f, 0.28f, 1.0f }; // Dark bluish background

        // Set viewport
        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = static_cast<float>(dx.width);
        vp.Height = static_cast<float>(dx.height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        dx.context->RSSetViewports(1, &vp);

        // Bind render target
        dx.context->OMSetRenderTargets(1, dx.rtv.GetAddressOf(), nullptr);

        // Clear
        dx.context->ClearRenderTargetView(dx.rtv.Get(), clearColor);

        // TODO: Draw calls will go here in later steps

        // Present
        dx.swapChain->Present(1, 0); // vsync on
    }

    // Cleanup
    dx.rtv.Reset();
    dx.swapChain.Reset();
    dx.context.Reset();
    dx.device.Reset();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}