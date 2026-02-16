#include "Engine/Renderer.h"
#include "Engine/ShaderManager.h"
#include "Engine/MeshManager.h"
#include "Engine/Components.h" // for CameraComponent & TransformComponent

using Microsoft::WRL::ComPtr;
using namespace DirectX;

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

        // Create initial resources: Rasterizer state, Depth/Stencil state, Constant Buffers, Sampler
        if (!CreateInitialResources())
            return false;

        // Create editor framebuffer (Render-to-Texture)
        if (!CreateFramebuffer(m_dx.width, m_dx.height))
            return false;

        return true;
    }

    void Renderer::Shutdown()
    {
        // Reset all ComPtrs (unload and cleanup combined)
        m_samplerState.Reset();
        m_depthStencilState.Reset();
        m_rasterState.Reset();
        m_skyboxDepthState.Reset();
        m_skyboxRasterState.Reset();
        m_skyboxSRV.Reset();

        m_cbLight.Reset();
        m_cbMaterial.Reset();

        m_cbWorld.Reset();
        m_cbView.Reset();
        m_cbProjection.Reset();

        // framebuffer state
        m_framebufferDSV.Reset();
        m_framebufferDepthTex.Reset();
        m_framebufferSRV.Reset();
        m_framebufferRTV.Reset();
        m_framebufferTex.Reset();

        ReleaseViews();

        m_dx.swapChain.Reset();
        m_dx.context.Reset();
        m_dx.device.Reset();
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
        if (!CreateViews())
            return false;

        // Resize editor framebuffer (Render-to-Texture)
        return CreateFramebuffer(m_dx.width, m_dx.height);
    }


    void Renderer::BeginFrame()
    {
        // Bind RTV/DSV and clear them
        m_dx.context->OMSetRenderTargets(1, m_dx.rtv.GetAddressOf(), m_dx.dsv.Get());

        const float clearColor[4] = { 0.10f, 0.18f, 0.28f, 1.0f };
        m_dx.context->ClearRenderTargetView(m_dx.rtv.Get(), clearColor);
        m_dx.context->ClearDepthStencilView(m_dx.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // viewport
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width    = static_cast<float>(m_dx.width);
        vp.Height   = static_cast<float>(m_dx.height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_dx.context->RSSetViewports(1, &vp);

        // basic states
        if (m_rasterState)       m_dx.context->RSSetState(m_rasterState.Get());
        if (m_depthStencilState) m_dx.context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

        // Bind per-frame CBs (b0=Proj, b1=View, b2=World)
        ID3D11Buffer* vscbs[] = { m_cbProjection.Get(), m_cbView.Get(), m_cbWorld.Get() };
        m_dx.context->VSSetConstantBuffers(0, 3, vscbs);
    }


    void Renderer::UpdateMatrixCB(ID3D11Buffer* cb, const XMMATRIX& m)
    {
        XMFLOAT4X4 rm;
        XMStoreFloat4x4(&rm, m);
        m_dx.context->UpdateSubresource(cb, 0, nullptr, &rm, 0, 0);
    }


    void Renderer::UpdateViewMatrix(const XMMATRIX& view)
    {
        if (m_cbView) UpdateMatrixCB(m_cbView.Get(), view);
    }


    void Renderer::UpdateProjectionMatrix(const XMMATRIX& proj)
    {
        if (m_cbProjection) UpdateMatrixCB(m_cbProjection.Get(), proj);
    }


    void Renderer::UpdateWorldMatrix(const XMMATRIX& world)
    {
        if (m_cbWorld) UpdateMatrixCB(m_cbWorld.Get(), world);
    }


    void Renderer::BindShader(const Engine::ShaderManager& shaderMan, int shaderID)
    {
        shaderMan.Bind(shaderID, m_dx.context.Get());
    }


    void Renderer::SubmitMesh(const Engine::MeshBuffers& mesh, ID3D11InputLayout* inputLayout)
    {
        UINT stride = mesh.stride;
        UINT offset = 0;
        m_dx.context->IASetInputLayout(inputLayout);
        m_dx.context->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &stride, &offset);
        m_dx.context->IASetIndexBuffer(mesh.indexBuffer, mesh.indexFormat, 0);
		m_dx.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);    // specifies how to interpret vertex data; every 3 vertices form a triangle
    }


    void Renderer::DrawIndexed(UINT indexCount)
    {
        m_dx.context->DrawIndexed(indexCount, 0, 0);
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

    void Renderer::UpdateLightConstants(const LightConstants& data)
    {
        if (m_cbLight)
        {
			// Upload light data to GPU
            m_dx.context->UpdateSubresource(m_cbLight.Get(), 0, nullptr, &data, 0, 0);
            // Bind to PS at slot b3 (matches HLSL CB_Light : register(b3))
            ID3D11Buffer* cbs[] = { m_cbLight.Get() };
            m_dx.context->PSSetConstantBuffers(3, 1, cbs);
        }
    }

    void Renderer::UpdateMaterialConstants(const MaterialConstants& material)
    {
        if (m_cbMaterial)
        {
            m_dx.context->UpdateSubresource(m_cbMaterial.Get(), 0, nullptr, &material, 0, 0);
            // Bind to PS at slot b4 (matches HLSL CB_Material : register(b4))
            ID3D11Buffer* cbs[] = { m_cbMaterial.Get() };
            m_dx.context->PSSetConstantBuffers(4, 1, cbs);
        }
    }

    bool Renderer::CreateInitialResources()
    {
        // Rasterizer state
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_BACK;
        rsDesc.FrontCounterClockwise = FALSE;   // clockwise vertices are front-facing
        rsDesc.DepthClipEnable = TRUE;
        HRESULT hr = m_dx.device->CreateRasterizerState(&rsDesc, m_rasterState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Depth-stencil state
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;     // enable writes to depth buffer
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS;               // standard depth test
        dsDesc.StencilEnable = FALSE;
        hr = m_dx.device->CreateDepthStencilState(&dsDesc, m_depthStencilState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Skybox Depth state: less-equal for z=w trick
        D3D11_DEPTH_STENCIL_DESC skyDsDesc = dsDesc;
        skyDsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = m_dx.device->CreateDepthStencilState(&skyDsDesc, m_skyboxDepthState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Skybox Raster state: disable culling to avoid winding issues
        D3D11_RASTERIZER_DESC skyRsDesc = rsDesc;
        skyRsDesc.CullMode = D3D11_CULL_NONE;
        hr = m_dx.device->CreateRasterizerState(&skyRsDesc, m_skyboxRasterState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Constant buffers: Projection(b0), View(b1), World(b2)
        if (!CreateMatrixCB(m_cbProjection.GetAddressOf())) return false;
        if (!CreateMatrixCB(m_cbView.GetAddressOf())) return false;
        if (!CreateMatrixCB(m_cbWorld.GetAddressOf())) return false;

        // Sampler state for PS s0
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;      // linear filtering
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;         // wrap texture coordinates
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.MipLODBias = 0.0f;                             // no LOD bias
        sampDesc.MaxAnisotropy = 1;                             // not using anisotropic filtering
        sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;      // no comparison
        sampDesc.BorderColor[0] = 0.0f;                         // border color (not used with wrap mode)
        sampDesc.BorderColor[1] = 0.0f;
        sampDesc.BorderColor[2] = 0.0f;
        sampDesc.BorderColor[3] = 0.0f;
        sampDesc.MinLOD = 0.0f;                                 // allow highest detail
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;                    // allow all mip levels

        hr = m_dx.device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf());
        if (FAILED(hr)) return false;

        // Light constant buffer (PS b3)
        {
            D3D11_BUFFER_DESC cb = {};
            cb.Usage = D3D11_USAGE_DEFAULT;
            cb.ByteWidth = static_cast<UINT>(sizeof(LightConstants));
            cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cb.CPUAccessFlags = 0;
            cb.MiscFlags = 0;

            // Ensure 16-byte multiple
            cb.ByteWidth = (cb.ByteWidth + 15) & ~15u;

            hr = m_dx.device->CreateBuffer(&cb, nullptr, m_cbLight.GetAddressOf());
            if (FAILED(hr)) return false;
        }

        // Material constant buffer (PS b4)
        {
            D3D11_BUFFER_DESC cb = {};
            cb.Usage = D3D11_USAGE_DEFAULT;
            cb.ByteWidth = static_cast<UINT>(sizeof(MaterialConstants)); // sizeof(MaterialConstants) should be 16
            cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cb.CPUAccessFlags = 0;
            cb.MiscFlags = 0;

            // Ensure 16-byte multiple
            cb.ByteWidth = (cb.ByteWidth + 15) & ~15u;

            hr = m_dx.device->CreateBuffer(&cb, nullptr, m_cbMaterial.GetAddressOf());
            if (FAILED(hr)) return false;
        }

        return true;
    }

    bool Renderer::CreateFramebuffer(UINT width, UINT height)
    {
        if (!m_dx.device || !m_dx.context)
            return false;

        // Safe to call during resize; release old resources first
        m_framebufferDSV.Reset();
        m_framebufferDepthTex.Reset();
        m_framebufferSRV.Reset();
        m_framebufferRTV.Reset();
        m_framebufferTex.Reset();

        // Color texture
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        HRESULT hr = m_dx.device->CreateTexture2D(&texDesc, nullptr, m_framebufferTex.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = m_dx.device->CreateRenderTargetView(m_framebufferTex.Get(), nullptr, m_framebufferRTV.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = m_dx.device->CreateShaderResourceView(m_framebufferTex.Get(), nullptr, m_framebufferSRV.GetAddressOf());
        if (FAILED(hr)) return false;

        // Depth texture
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthDesc.CPUAccessFlags = 0;
        depthDesc.MiscFlags = 0;

        hr = m_dx.device->CreateTexture2D(&depthDesc, nullptr, m_framebufferDepthTex.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = m_dx.device->CreateDepthStencilView(m_framebufferDepthTex.Get(), nullptr, m_framebufferDSV.GetAddressOf());
        if (FAILED(hr)) return false;

        return true;
    }


    void Renderer::BindFramebuffer()
    {
        if (!m_dx.context || !m_framebufferRTV || !m_framebufferDSV)
            return;

        m_dx.context->OMSetRenderTargets(1, m_framebufferRTV.GetAddressOf(), m_framebufferDSV.Get());

        // viewport (match framebuffer)
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width    = static_cast<float>(m_dx.width);
        vp.Height   = static_cast<float>(m_dx.height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_dx.context->RSSetViewports(1, &vp);

        // clear to a dark grey editor background
        const float clearColor[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
        m_dx.context->ClearRenderTargetView(m_framebufferRTV.Get(), clearColor);
        m_dx.context->ClearDepthStencilView(m_framebufferDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // keep states consistent with BeginFrame()
        if (m_rasterState)       m_dx.context->RSSetState(m_rasterState.Get());
        if (m_depthStencilState) m_dx.context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

        // Bind per-frame CBs (b0=Proj, b1=View, b2=World)
        ID3D11Buffer* vscbs[] = { m_cbProjection.Get(), m_cbView.Get(), m_cbWorld.Get() };
        m_dx.context->VSSetConstantBuffers(0, 3, vscbs);
    }


    void Renderer::BindBackBuffer()
    {
        if (!m_dx.context || !m_dx.rtv || !m_dx.dsv)
            return;

        m_dx.context->OMSetRenderTargets(1, m_dx.rtv.GetAddressOf(), m_dx.dsv.Get());

        // viewport (match window)
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width    = static_cast<float>(m_dx.width);
        vp.Height   = static_cast<float>(m_dx.height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_dx.context->RSSetViewports(1, &vp);

        // clear main back buffer to pure black
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_dx.context->ClearRenderTargetView(m_dx.rtv.Get(), clearColor);
        m_dx.context->ClearDepthStencilView(m_dx.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // keep states consistent with BeginFrame()
        if (m_rasterState)       m_dx.context->RSSetState(m_rasterState.Get());
        if (m_depthStencilState) m_dx.context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

        // Bind per-frame CBs (b0=Proj, b1=View, b2=World)
        ID3D11Buffer* vscbs[] = { m_cbProjection.Get(), m_cbView.Get(), m_cbWorld.Get() };
        m_dx.context->VSSetConstantBuffers(0, 3, vscbs);
    }


    void Renderer::DrawSkybox(const Engine::MeshManager& meshMan, const Engine::ShaderManager& shaderMan, const Engine::CameraComponent& camComp, const Engine::TransformComponent& camTrans)
    {
        if (!m_skyboxSRV) return;

        auto* ctx = m_dx.context.Get();

        // States for skybox
        if (m_skyboxDepthState) ctx->OMSetDepthStencilState(m_skyboxDepthState.Get(), 0);
        if (m_skyboxRasterState) ctx->RSSetState(m_skyboxRasterState.Get());

		// Skybox world: identity (scaled to ensure it's not clipped by near plane)
        DirectX::XMMATRIX world = DirectX::XMMatrixScaling(50.0f, 50.0f, 50.0f);

        // Build view rotation only (strip translation)
        // Use Transpose(R) == Inverse(R) for pure rotation for numerical stability
        DirectX::XMVECTOR qn = DirectX::XMLoadFloat4(&camTrans.rotation);
        qn = DirectX::XMQuaternionNormalize(qn);
        DirectX::XMMATRIX viewRotOnly = DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationQuaternion(qn));

        // Projection using camera FOV and current aspect
        float aspect = static_cast<float>(m_dx.width) / static_cast<float>(m_dx.height ? m_dx.height : 1u);
        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(camComp.FOV, aspect, camComp.nearClip, camComp.farClip);

        // Update CBs used by SkyboxVS
        UpdateWorldMatrix(world);
        UpdateViewMatrix(viewRotOnly);
        UpdateProjectionMatrix(proj);

        // Bind skybox shaders (shaderID 2 reserved for skybox)
        shaderMan.Bind(2, ctx);

        // Bind sampler and cubemap SRV
        ID3D11SamplerState* sampler = GetSamplerState();
        if (sampler) ctx->PSSetSamplers(0, 1, &sampler);
        ID3D11ShaderResourceView* srv = m_skyboxSRV.Get();
        ctx->PSSetShaderResources(0, 1, &srv);

        // Draw cube mesh (ID 101)
        Engine::MeshBuffers cube{};
        if (meshMan.GetMesh(101, cube))
        {
            ID3D11InputLayout* layout = shaderMan.GetInputLayout(2);
            SubmitMesh(cube, layout);
            DrawIndexed(cube.indexCount);
        }
        else {
			throw std::runtime_error("Skybox cube mesh (ID 101) not found in MeshManager.");
        }

        // Restore default states (so subsequent draws aren't affected)
        if (m_depthStencilState) ctx->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
        if (m_rasterState)       ctx->RSSetState(m_rasterState.Get());
    }
}