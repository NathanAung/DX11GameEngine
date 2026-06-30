#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Engine/TextureManager.h"
#include "Engine/AssetManager.h"
#include <vector>
#include <sstream>
#include <iostream>

using Microsoft::WRL::ComPtr;

namespace Engine
{
    UUID TextureManager::LoadTexture(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filepath)
    {
        // Register file to get its UUID
        UUID assetID = assetManager.ImportAsset(filepath, Engine::AssetType::Texture);

        // Memory Pool Check (Bypass disk I/O if already loaded)
        if (m_textures.find(assetID) != m_textures.end()) return assetID;

        // Load Image Data (force RGBA)
        int width = 0, height = 0, channels = 0;
        stbi_set_flip_vertically_on_load(true); // Flip for DirectX UVs
        unsigned char* imgData = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
        if (!imgData)
        {
            std::cerr << "TextureManager Failed to load: " << filepath << std::endl;
            return m_defaultUUID;   // fallback to white pixel if loading fails
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
        initData.pSysMem = imgData;
        initData.SysMemPitch = static_cast<UINT>(width * 4);    // 4 bytes per pixel (RGBA)
        initData.SysMemSlicePitch = 0;  // the size of one depth slice for 3D textures, not used here since it's 2D

        // Create Texture2D
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, texture.GetAddressOf());

        // Cleanup staging data immediately
        stbi_image_free(imgData);

        if (FAILED(hr))
        {
            return m_defaultUUID;   // fallback to white pixel if creation fails
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
            return m_defaultUUID;   // fallback to white pixel if SRV creation fails
        }

        // Register safely in memory pool (ComPtr increments reference count here)
        m_textures[assetID] = srv;
        assetManager.SetAssetLoaded(assetID, true);

        return assetID;
    }


    UUID TextureManager::CreateDefaultTexture(ID3D11Device* device, Engine::AssetManager& assetManager)
    {
        // Register a virtual path for the primitive texture
        // a primitive path is used to avoid conflicts with real file paths
        UUID assetID = assetManager.ImportAsset("primitive://default_texture", Engine::AssetType::Texture);

        // Memory Pool Check
        if (m_textures.find(assetID) != m_textures.end()) return assetID;

        // 1x1 White Pixel (RGBA: 255, 255, 255, 255)
        const uint32_t pixel = 0xFFFFFFFF;

        // Texture Description
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // Initialize with the white pixel data
        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = &pixel;
        initData.SysMemPitch = sizeof(uint32_t);    // 4 bytes for 1 pixel
        initData.SysMemSlicePitch = 0;

        // Create Texture2D
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, texture.GetAddressOf());
        if (FAILED(hr))
        {
            std::cerr << "Failed to create default texture." << std::endl;
            return 0;   // fallback to invalid UUID if creation fails
        }

        // Create Shader Resource View (SRV) for the texture
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
            std::cerr << "Failed to create default texture SRV." << std::endl;
            return 0;   // fallback to invalid UUID if SRV creation fails
        }

        // Register in memory pool
        m_textures[assetID] = srv;
        m_defaultSRV = srv;
        m_defaultUUID = assetID;

        assetManager.SetAssetLoaded(assetID, true);

        return assetID;
    }


    UUID TextureManager::LoadCubemap(ID3D11Device* device, Engine::AssetManager& assetManager, const std::vector<std::string>& filepaths)
    {
        // Expect exactly 6 faces: +X, -X, +Y, -Y, +Z, -Z
        if (filepaths.size() != 6) return m_defaultUUID;

        // Use the first face's path as the identifier for the whole cubemap
        std::string virtualPath = "cubemap://" + filepaths[0];
        UUID assetID = assetManager.ImportAsset(virtualPath, Engine::AssetType::Texture);

        // Memory Pool Check
        if (m_textures.find(assetID) != m_textures.end()) return assetID;

        // Load from Disk
        stbi_set_flip_vertically_on_load(false);    // Cubemaps don't flip

        // D3D11 Texture Description for Cubemap
        D3D11_TEXTURE2D_DESC desc{};
        // subresource data array for each face of the cubemap
        D3D11_SUBRESOURCE_DATA data[6]{};
        // Array to hold the loaded image data for each face
        unsigned char* images[6] = { nullptr };

        // Load each face of the cubemap
        for (int i = 0; i < 6; ++i)
        {
            int w, h, c;
            images[i] = stbi_load(filepaths[i].c_str(), &w, &h, &c, 4);
            if (!images[i])
            {
                std::cerr << "TextureManager Failed to load cubemap face: " << filepaths[i] << std::endl;
                for (int j = 0; j < i; ++j) stbi_image_free(images[j]); // Cleanup previous faces
                return m_defaultUUID;
            }

            // Set the texture description only once, using the dimensions of the first face
            if (i == 0)
            {
                desc.Width = w;
                desc.Height = h;
                desc.MipLevels = 1;
                desc.ArraySize = 6;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;   // Crucial for Skyboxes
            }

            // Initialize subresource data for each face
            data[i].pSysMem = images[i];
            data[i].SysMemPitch = desc.Width * 4;
            data[i].SysMemSlicePitch = 0;
        }

        // Hardware Creation
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&desc, data, texture.GetAddressOf());

		// Cleanup staging resources
        for (int i = 0; i < 6; ++i) stbi_image_free(images[i]);

        if (FAILED(hr)) return m_defaultUUID;

        // Shader Resource View (SRV) Description for Cubemap
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = 1;

        // Create the SRV for the cubemap texture
        ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());

        if (FAILED(hr)) return m_defaultUUID;

        // Register in memory pool
        m_textures[assetID] = srv;
        assetManager.SetAssetLoaded(assetID, true);

        return assetID;
    }


    ID3D11ShaderResourceView* TextureManager::GetTexture(UUID uuid) const
    {
		// Simple hash map lookup
        auto it = m_textures.find(uuid);
        if (it != m_textures.end())
        {
            // Returns the raw pointer temporarily for binding to the DirectX pipeline
            return it->second.Get();
        }
        return m_defaultSRV.Get();
    }
}