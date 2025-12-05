#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Engine/TextureManager.h"
#include <vector>

using Microsoft::WRL::ComPtr;

namespace Engine
{
    ID3D11ShaderResourceView* TextureManager::LoadTexture(ID3D11Device* device, const std::string& filename)
    {
        // Check Cache
        auto it = m_textureCache.find(filename);
        if (it != m_textureCache.end())
        {
            return it->second.Get();
        }

        // Load Image Data (force RGBA)
        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &channels, 4);
        if (!pixels || width <= 0 || height <= 0)
        {
            if (pixels) stbi_image_free(pixels);
            return nullptr;
        }

        // Create D3D11 Texture
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = static_cast<UINT>(width);
        texDesc.Height = static_cast<UINT>(height);
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

		// Initialize with image data
        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = pixels;
        initData.SysMemPitch = static_cast<UINT>(width * 4);
        initData.SysMemSlicePitch = 0;

		// Create Texture2D
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, texture.GetAddressOf());

        // Cleanup image data as soon as GPU resource is created (or on failure)
        stbi_image_free(pixels);

        if (FAILED(hr))
        {
            return nullptr;
        }

        // Create Shader Resource View (SRV)
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());
        if (FAILED(hr))
        {
            return nullptr;
        }

        // Store in cache and return raw pointer
        m_textureCache.emplace(filename, srv);
        return srv.Get();
    }
}