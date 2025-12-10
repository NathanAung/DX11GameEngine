#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <vector>

// TextureManager class handles loading and caching of textures from files using the stb_image library.

namespace Engine
{
    class TextureManager
    {
    public:
        // Loads a texture and returns an SRV raw pointer. Cached by filename.
        // Returns nullptr on failure. Manager retains ownership via ComPtr.
        ID3D11ShaderResourceView* LoadTexture(ID3D11Device* device, const std::string& filename);

        // Loads a cubemap from 6 images: order = +X, -X, +Y, -Y, +Z, -Z
        // Returns SRV raw pointer for TextureCube, or nullptr on failure.
        ID3D11ShaderResourceView* LoadCubemap(ID3D11Device* device, const std::vector<std::string>& filenames);

    private:
        // Cache of loaded 2D textures: filename -> SRV
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;

        // Cache of cubemaps by concatenated key of 6 filenames
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_cubemapCache;
    };
}