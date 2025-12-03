#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h> // For ComPtr

namespace Engine
{
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

class Renderer
{
public:
    // High-level lifecycle methods
    bool InitD3D11(HWND hwnd, unsigned width, unsigned height);
    void Shutdown();
    void Present(bool vsync);
    bool Resize(unsigned width, unsigned height);

    // Resource Accessors (for Systems to use)
    ID3D11Device* GetDevice() const { return m_dx.device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_dx.context.Get(); }
    ID3D11Buffer* GetViewCB() const { return m_cbView.Get(); }
    ID3D11Buffer* GetProjectionCB() const { return m_cbProjection.Get(); }
    ID3D11Buffer* GetWorldCB() const { return m_cbWorld.Get(); }

    // Frame Setup Accessors (for the main Render function to use)
    ID3D11RenderTargetView* GetRTV() const { return m_dx.rtv.Get(); }
    ID3D11DepthStencilView* GetDSV() const { return m_dx.dsv.Get(); }
    ID3D11RasterizerState* GetRasterState() const { return m_rasterState.Get(); }
    ID3D11DepthStencilState* GetDepthStencilState() const { return m_depthStencilState.Get(); }
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
    // other global ComPtr resources? (shaders, input layout, etc.)

    // Helper for initial resource creation (Rasterizer, Depth/Stencil, CBs)
    bool CreateInitialResources();

    // Internal helpers
    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateViews();
    bool CreateMatrixCB(ID3D11Buffer** outBuffer);
    void ReleaseViews();
};

}