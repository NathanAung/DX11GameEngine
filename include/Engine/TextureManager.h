#pragma once
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "Engine/UUID.h"

// TextureManager class handles loading and caching of textures from files using the stb_image library.

namespace Engine
{
	class AssetManager;

    class TextureManager
    {
    public:
        TextureManager() = default;
        ~TextureManager();

        // Core Loaders
        // Creates a 1x1 white default texture
        UUID CreateDefaultTexture(ID3D11Device* device, Engine::AssetManager& assetManager);
        // Loads a texture and returns an SRV raw pointer. Cached by filename.
        UUID LoadTexture(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filepath);
        // Loads a cubemap from 6 images: order = +X, -X, +Y, -Y, +Z, -Z
        UUID LoadCubemap(ID3D11Device* device, Engine::AssetManager& assetManager, const std::vector<std::string>& filepaths);

		// Fetching uses the UUID to retrieve the corresponding SRV from the cache
        ID3D11ShaderResourceView* GetTexture(UUID uuid) const;
        ID3D11ShaderResourceView* GetDefaultTexture() const { return m_defaultSRV; }

    private:
        // The master memory pool for textures
        std::unordered_map<UUID, ID3D11ShaderResourceView*> m_textures;

		// Cache mapping filenames to UUIDs for quick lookup
        ID3D11ShaderResourceView* m_defaultSRV = nullptr;
        UUID m_defaultUUID = 0;
    };
}