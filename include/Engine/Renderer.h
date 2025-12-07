#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h> // For ComPtr
#include <DirectXMath.h>

// The Renderer class encapsulates DirectX 11 rendering functionality
// Flow of operations: InitD3D11 -> BeginFrame -> [Update... / Bind... / Submit...] -> DrawIndexed -> Present -> Shutdown

namespace Engine
{
struct MeshBuffers;
class ShaderManager;

// Encapsulates DirectX 11 device, context, swap chain, and views
struct DX11Context
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    UINT width = 1280;
    UINT height = 720;
};

// Light constant buffer layout (aligned for HLSL cbuffer packing)
// padding added to ensure 16-byte alignment
struct LightConstants
{
    DirectX::XMFLOAT3 dir;      
    float padding1;   // pad to 16 bytes
    DirectX::XMFLOAT3 color;    
    float intensity;  // pack intensity as scalar
    DirectX::XMFLOAT3 cameraPos;
    float padding2;   // pad to 16 bytes
};

class Renderer
{
public:
    // High-level lifecycle methods
    bool InitD3D11(HWND hwnd, unsigned width, unsigned height);
    void Shutdown();
    void Present(bool vsync);
    bool Resize(unsigned width, unsigned height);

    // Frame methods and D3D11 command helpers
    
    // when starting a new frame, clears RTV/DSV
    void BeginFrame();
    void UpdateViewMatrix(const DirectX::XMMATRIX& view);
    void UpdateProjectionMatrix(const DirectX::XMMATRIX& proj);
    void UpdateWorldMatrix(const DirectX::XMMATRIX& world);
    // upload light constants to GPU
    void UpdateLightConstants(const LightConstants& data);
    // Binds shaders from ShaderManager
    void BindShader(const Engine::ShaderManager& shaderMan, int shaderID);
    // Submits mesh buffers for drawing
    void SubmitMesh(const Engine::MeshBuffers& mesh, ID3D11InputLayout* inputLayout);
    // Issues the draw call
    void DrawIndexed(UINT indexCount);

    // Resource Accessors (for Systems to use if needed)
    ID3D11Device* GetDevice() const { return m_dx.device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_dx.context.Get(); }

    // Frame Setup Accessors
    ID3D11RenderTargetView* GetRTV() const { return m_dx.rtv.Get(); }
    ID3D11DepthStencilView* GetDSV() const { return m_dx.dsv.Get(); }
    ID3D11RasterizerState* GetRasterState() const { return m_rasterState.Get(); }
    ID3D11DepthStencilState* GetDepthStencilState() const { return m_depthStencilState.Get(); }
    ID3D11SamplerState* GetSamplerState() const { return m_samplerState.Get(); }
	ID3D11Buffer* GetLightCB() const { return m_cbLight.Get(); } // light cbuffer
    UINT GetWidth() const { return m_dx.width; }
    UINT GetHeight() const { return m_dx.height; }

private:
    DX11Context m_dx;

    // DirectX ComPtr globals
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbProjection;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbView;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbWorld;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbLight; // light cbuffer

    // Helper for initial resource creation (Rasterizer, Depth/Stencil, CBs)
    bool CreateInitialResources();

    // Internal helpers
    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateViews();
    bool CreateMatrixCB(ID3D11Buffer** outBuffer);
    void ReleaseViews();

    // matrix update helper
    void UpdateMatrixCB(ID3D11Buffer* cb, const DirectX::XMMATRIX& m);
};

}