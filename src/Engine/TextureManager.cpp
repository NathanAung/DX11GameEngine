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
        //stbi_set_flip_vertically_on_load(true); // Flip for DirectX UVs

        unsigned char* imgData = nullptr;

        // Route the loading through the VFS if active
        if (assetManager.IsVFSActive())
        {
			// UUID assigned to this file path is used to locate the asset in the VFS
            Engine::UUID handle = assetManager.ImportAsset(filepath, Engine::AssetType::Texture);
            std::vector<char> memBuffer = assetManager.ReadAssetFromVFS(handle);

            if (!memBuffer.empty())
            {
                imgData = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(memBuffer.data()),
                    static_cast<int>(memBuffer.size()),
                    &width, &height, &channels, 4
                );
            }
        }
        else
        {
            // Standard Editor fallback
            imgData = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
        }

        if (!imgData)
        {
            std::cerr << "TextureManager Failed to load: " << filepath << std::endl;
            return m_defaultTextureID;   // fallback to white pixel if loading fails
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
            return m_defaultTextureID;   // fallback to white pixel if creation fails
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
            return m_defaultTextureID;   // fallback to white pixel if SRV creation fails
        }

        // Register safely in memory pool (ComPtr increments reference count here)
        m_textures[assetID] = srv;
        assetManager.SetAssetLoaded(assetID, true);

        return assetID;
    }


    UUID TextureManager::CreateDefaultTexture(ID3D11Device* device, Engine::AssetManager& assetManager)
    {
        // Default Albedo (White)
        m_defaultTextureID = Create1x1Texture(device, assetManager, "primitive://white", 255, 255, 255, 255);

        // Default Normal Map (Flat Tangent-Space Z-up)
        // RGB(128, 128, 255) maps mathematically to XYZ(0, 0, 1)
        m_defaultNormalID = Create1x1Texture(device, assetManager, "primitive://default_normal", 128, 128, 255, 255);

        // Default ORM Map (Occlusion, Roughness, Metallic)
        // Occlusion = 255 (Full white, no shadowing)
        // Roughness = 128 (Mid-gray, 50% rough)
        // Metallic  = 0   (Black, fully dielectric/non-metal)
        m_defaultORMID = Create1x1Texture(device, assetManager, "primitive://default_orm", 255, 128, 0, 255);

        // Keep the legacy fallback pointer assigned just in case GetTexture() needs it
        if (m_textures.find(m_defaultTextureID) != m_textures.end()) {
            m_defaultSRV = m_textures[m_defaultTextureID];
        }

        return m_defaultTextureID;
    }


    UUID TextureManager::LoadCubemap(ID3D11Device* device, Engine::AssetManager& assetManager, const std::vector<std::string>& filepaths)
    {
        // Expect exactly 6 faces: +X, -X, +Y, -Y, +Z, -Z
        if (filepaths.size() != 6) return m_defaultTextureID;

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
            // Register the individual physical face so the Packer knows about it
            Engine::UUID faceUUID = assetManager.ImportAsset(filepaths[i], Engine::AssetType::Texture);

            int w, h, c;

            // VFS Memory routing for each face
            if (assetManager.IsVFSActive())
            {
                std::vector<char> memBuffer = assetManager.ReadAssetFromVFS(faceUUID);
                if (!memBuffer.empty())
                {
                    images[i] = stbi_load_from_memory(
                        reinterpret_cast<const stbi_uc*>(memBuffer.data()),
                        static_cast<int>(memBuffer.size()),
                        &w, &h, &c, 4
                    );
                }
            }
			// Standard Editor fallback
            else {
                images[i] = stbi_load(filepaths[i].c_str(), &w, &h, &c, 4);
            }
            
            if (!images[i])
            {
                std::cerr << "TextureManager Failed to load cubemap face: " << filepaths[i] << std::endl;
                for (int j = 0; j < i; ++j) stbi_image_free(images[j]); // Cleanup previous faces
                return m_defaultTextureID;
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

        if (FAILED(hr)) return m_defaultTextureID;

        // Shader Resource View (SRV) Description for Cubemap
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = 1;

        // Create the SRV for the cubemap texture
        ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());

        if (FAILED(hr)) return m_defaultTextureID;

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


    UUID TextureManager::Create1x1Texture(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& virtualPath, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        UUID assetID = assetManager.ImportAsset(virtualPath, Engine::AssetType::Texture);
        if (m_textures.find(assetID) != m_textures.end()) return assetID;

        // 1 pixel color data
        uint32_t pixel = (a << 24) | (b << 16) | (g << 8) | r;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = &pixel;
        initData.SysMemPitch = 4; // 4 bytes per row

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
        if (FAILED(hr)) return 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // Store directly into a ComPtr instead of TextureData
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, srv.GetAddressOf());
        if (FAILED(hr)) return 0;

        m_textures[assetID] = srv;
        assetManager.SetAssetLoaded(assetID, true);

        return assetID;
    }
}