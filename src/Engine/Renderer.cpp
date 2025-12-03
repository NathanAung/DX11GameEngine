#include "Engine/Renderer.h"
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace Engine
{
    bool Renderer::InitD3D11(HWND hwnd, unsigned width, unsigned height)
    {
        m_dx.width = width;
        m_dx.height = height;

		// Create device, context, and swap chain
        if (!CreateDeviceAndSwapChain(hwnd))
            return false;

        // Create the render target view for the back buffer
        if (!CreateViews())
            return false;

		// Create initial resources: Rasterizer state, Depth/Stencil state, Constant Buffers
        if (!CreateInitialResources())
            return false;

        return true;
    }

    void Renderer::Shutdown()
    {
        // Reset all ComPtrs (unload and cleanup combined)
        m_depthStencilState.Reset();
        m_rasterState.Reset();

        m_cbWorld.Reset();
        m_cbView.Reset();
        m_cbProjection.Reset();

        ReleaseViews();

        if (m_dx.swapChain) m_dx.swapChain.Reset();
        if (m_dx.context)   m_dx.context.Reset();
        if (m_dx.device)    m_dx.device.Reset();
    }

    void Renderer::Present(bool vsync)
    {
        // Present back buffer
        if (m_dx.swapChain)
            m_dx.swapChain->Present(vsync ? 1 : 0, 0);
    }

    bool Renderer::Resize(unsigned width, unsigned height)
    {
        if (width == 0 || height == 0)
            return true; // minimized, ignore

        if (!m_dx.swapChain || !m_dx.context)
            return false;

        m_dx.width = width;
        m_dx.height = height;

        // Unbind and release current views
        m_dx.context->OMSetRenderTargets(0, nullptr, nullptr);
        ReleaseViews();

        // Resize the swap chain buffers
        HRESULT hr = m_dx.swapChain->ResizeBuffers(0, m_dx.width, m_dx.height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return false;

        // Recreate render target and depth-stencil views
        return CreateViews();
    }

    bool Renderer::CreateDeviceAndSwapChain(HWND hwnd)
    {
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
        sd.BufferDesc.Width = m_dx.width;
        sd.BufferDesc.Height = m_dx.height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;      // 32-bit color format (8 bits for red, green, blue, and alpha channels)
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;       // Use the back buffer as a render target
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;                                // number of samples per pixel, no multi-sampling
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
            nullptr,                        // Use default adapter
            D3D_DRIVER_TYPE_HARDWARE,       // Hardware driver
            nullptr,                        // No software rasterizer
            createDeviceFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &sd,
            swapChain.GetAddressOf(),
            device.GetAddressOf(),
            &m_dx.featureLevel,
            context.GetAddressOf());
        if (FAILED(hr)) return false;

        m_dx.device = device;
        m_dx.context = context;
        m_dx.swapChain = swapChain;
        return true;
    }

    bool Renderer::CreateViews()
    {
        // Release existing RTV/DSV if any
        ReleaseViews();

        // back buffer texture which will be used to create the RTV
        ComPtr<ID3D11Texture2D> backBuffer;
        // Get the back buffer from the swap chain
        HRESULT hr = m_dx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
        if (FAILED(hr)) return false;

        // Create the render target view from the back buffer
        hr = m_dx.device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_dx.rtv.GetAddressOf());
        if (FAILED(hr)) return false;

        // Create depth-stencil buffer and view
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = m_dx.width;                       // Match the swap chain dimensions
        depthDesc.Height = m_dx.height;
        depthDesc.MipLevels = 1;                            // No mipmaps
        depthDesc.ArraySize = 1;                            // Single texture
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;   // 24 bits for depth, 8 bits for stencil
        depthDesc.SampleDesc.Count = 1;                     // No multisampling
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;              // GPU read/write
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;     // Bind as depth-stencil buffer

        // Create the depth-stencil texture
        hr = m_dx.device->CreateTexture2D(&depthDesc, nullptr, m_dx.depthStencilBuffer.GetAddressOf());
        if (FAILED(hr)) return false;

        // Create the depth-stencil view
        hr = m_dx.device->CreateDepthStencilView(m_dx.depthStencilBuffer.Get(), nullptr, m_dx.dsv.GetAddressOf());
        if (FAILED(hr)) return false;

        return true;
    }

    void Renderer::ReleaseViews()
    {
        m_dx.dsv.Reset();
        m_dx.depthStencilBuffer.Reset();
        m_dx.rtv.Reset();
    }

    bool Renderer::CreateMatrixCB(ID3D11Buffer** outBuffer)
    {
        D3D11_BUFFER_DESC cb = {};
        cb.Usage = D3D11_USAGE_DEFAULT;
        cb.ByteWidth = sizeof(DirectX::XMFLOAT4X4);
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = 0;

        ComPtr<ID3D11Buffer> buf;
        HRESULT hr = m_dx.device->CreateBuffer(&cb, nullptr, buf.GetAddressOf());
        if (FAILED(hr)) return false;

        *outBuffer = buf.Detach();
        return true;
    }

    bool Renderer::CreateInitialResources()
    {
        // Rasterizer state
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_BACK;
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthClipEnable = TRUE;
        HRESULT hr = m_dx.device->CreateRasterizerState(&rsDesc, m_rasterState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Depth-stencil state
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
        dsDesc.StencilEnable = FALSE;
        hr = m_dx.device->CreateDepthStencilState(&dsDesc, m_depthStencilState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Constant buffers: Projection(b0), View(b1), World(b2)
        if (!CreateMatrixCB(m_cbProjection.GetAddressOf())) return false;
        if (!CreateMatrixCB(m_cbView.GetAddressOf())) return false;
        if (!CreateMatrixCB(m_cbWorld.GetAddressOf())) return false;

        return true;
    }
}