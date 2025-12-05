#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>

// TextureManager class handles loading and caching of textures from files using the stb_image library.

namespace Engine
{
    class TextureManager
    {
    public:
        // Loads a texture and returns an SRV raw pointer. Cached by filename.
        // Returns nullptr on failure. Manager retains ownership via ComPtr.
        ID3D11ShaderResourceView* LoadTexture(ID3D11Device* device, const std::string& filename);

    private:
		// Cache of loaded textures: filename -> SRV
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;
    };
}