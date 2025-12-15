#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Engine/TextureManager.h"
#include <vector>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace Engine
{
    ID3D11ShaderResourceView* TextureManager::LoadTexture(ID3D11Device* device, const std::string& filename)
    {
		// Check Cache, return if found
        auto it = m_textureCache.find(filename);
        if (it != m_textureCache.end())
        {
            return it->second.Get();
        }

        // Load Image Data (force RGBA)
        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &channels, 4);
		if (!pixels || width <= 0 || height <= 0)   // failed to load
        {
            if (pixels) stbi_image_free(pixels);
            return nullptr;
        }

		// D3D11 Texture Description
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
		initData.SysMemPitch = static_cast<UINT>(width * 4);    // 4 bytes per pixel (RGBA)
		initData.SysMemSlicePitch = 0;  // the size of one depth slice for 3D textures, not used here since it's 2D

		// Create Texture2D
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, texture.GetAddressOf());

        // Cleanup image data as soon as GPU resource is created (or on failure)
        stbi_image_free(pixels);

        if (FAILED(hr))
        {
            return nullptr;
        }

		// Shader Resource View (SRV) Description
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        // Create the SRV for the texture
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

    ID3D11ShaderResourceView* TextureManager::LoadCubemap(ID3D11Device* device, const std::vector<std::string>& filenames)
    {
        // Expect exactly 6 faces: +X, -X, +Y, -Y, +Z, -Z
        if (filenames.size() != 6)
            return nullptr;

        // Build cache key from filenames
        std::ostringstream oss;
        for (const auto& f : filenames) { oss << f << "|"; }
        const std::string key = oss.str();

        // Check cubemap cache
        if (auto it = m_cubemapCache.find(key); it != m_cubemapCache.end())
        {
            return it->second.Get();
        }

        // Load all 6 faces
        stbi_uc* faces[6] = {};
        int width = 0, height = 0, channels = 0;

        for (int i = 0; i < 6; ++i)
        {
            int w = 0, h = 0, c = 0;
            faces[i] = stbi_load(filenames[i].c_str(), &w, &h, &c, 4);
            if (!faces[i] || w <= 0 || h <= 0)
            {
                // Cleanup any loaded faces
                for (int j = 0; j < 6; ++j)
                {
                    if (faces[j]) stbi_image_free(faces[j]);
                }
                return nullptr;
            }

			// On first face, record dimensions
            if (i == 0)
            {
                width = w; height = h; channels = 4;
            }
            else
            {
                // Ensure consistent dimensions
                if (w != width || h != height)
                {
                    for (int j = 0; j < 6; ++j)
                    {
                        if (faces[j]) stbi_image_free(faces[j]);
                    }
                    return nullptr;
                }
            }
        }

        // Describe TextureCube (2D array with 6 slices)
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = static_cast<UINT>(width);
        texDesc.Height = static_cast<UINT>(height);
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 6;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        // Subresource data for 6 faces
        D3D11_SUBRESOURCE_DATA initData[6]{};
        for (int i = 0; i < 6; ++i)
        {
            initData[i].pSysMem = faces[i];
            initData[i].SysMemPitch = static_cast<UINT>(width * 4);
            initData[i].SysMemSlicePitch = 0;
        }

        // Create Texture2D with 6 subresources
        ComPtr<ID3D11Texture2D> texCube;
        HRESULT hr = device->CreateTexture2D(&texDesc, initData, texCube.GetAddressOf());

        // Free pixel data
        for (int i = 0; i < 6; ++i)
        {
            if (faces[i]) stbi_image_free(faces[i]);
        }

        if (FAILED(hr))
        {
            return nullptr;
        }

        // Create SRV for TextureCube
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = 1;

        ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(texCube.Get(), &srvDesc, srv.GetAddressOf());
        if (FAILED(hr))
        {
            return nullptr;
        }

        // Cache and return
        m_cubemapCache.emplace(key, srv);
        return srv.Get();
    }
}