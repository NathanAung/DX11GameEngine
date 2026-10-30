#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include <stb_truetype.h>
#include "Engine/UUID.h"

namespace Engine
{
    class AssetManager;

	// FontAsset holds the baked font texture atlas and character data for rendering text
    struct FontAsset
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> atlasSRV;
        stbtt_bakedchar charData[96]; // Data for ASCII characters 32 through 126
        int atlasWidth = 512;
        int atlasHeight = 512;
        float fontSize = 32.0f;
    };

    class FontManager
    {
    public:
        // Loads a TTF file, bakes it into a texture atlas, and uploads it to the GPU
        UUID LoadFont(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filepath, float fontSize);

        // Retrieves the baked font asset
        FontAsset* GetFont(UUID fontID);

        void Shutdown() { m_fonts.clear(); }

    private:
        std::unordered_map<UUID, FontAsset> m_fonts;
    };
}