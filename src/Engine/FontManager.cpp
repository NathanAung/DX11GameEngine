#define STB_TRUETYPE_IMPLEMENTATION
#include "Engine/FontManager.h"
#include "Engine/AssetManager.h"
#include <fstream>
#include <stdexcept>

namespace Engine
{
    UUID FontManager::LoadFont(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filepath, float fontSize)
    {
        // Generate a unique virtual path incorporating the font size
        std::string virtualPath = filepath + "?size=" + std::to_string(fontSize);
        UUID assetID = assetManager.ImportAsset(virtualPath, Engine::AssetType::Font);

        // Return cached font if already loaded
        if (m_fonts.find(assetID) != m_fonts.end()) return assetID;

        // Load the raw TTF bytes (Checking VFS for game builds, or reading from disk for Editor builds)
        Engine::UUID fileID = assetManager.ImportAsset(filepath, Engine::AssetType::Font);
        std::vector<char> ttfBuffer;
        if (assetManager.IsVFSActive())
        {
            ttfBuffer = assetManager.ReadAssetFromVFS(fileID);
        }
        else
        {
            std::ifstream file(filepath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) throw std::runtime_error("Failed to open font file: " + filepath);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            ttfBuffer.resize(size);
            file.read(ttfBuffer.data(), size);
        }

		// Prepare the FontAsset structure
        FontAsset font;
        font.fontSize = fontSize;
        font.atlasWidth = 512;
        font.atlasHeight = 512;

        // Bake the TTF into a 1-channel grayscale/alpha bitmap
        std::vector<unsigned char> tempBitmap(font.atlasWidth * font.atlasHeight);
        stbtt_BakeFontBitmap(reinterpret_cast<const unsigned char*>(ttfBuffer.data()), 0, fontSize,
            tempBitmap.data(), font.atlasWidth, font.atlasHeight,
            32, 96, font.charData);

        // Expand the 1-channel bitmap into a 4-channel 32-bit RGBA texture
        // We set RGB to pure white (255) and use the baked bitmap purely for the Alpha channel.
        // This allows SpritePS.hlsl to dynamically tint the text to any color.
        std::vector<unsigned char> rgbaBitmap(font.atlasWidth * font.atlasHeight * 4);
        for (size_t i = 0; i < tempBitmap.size(); ++i)
        {
            rgbaBitmap[i * 4 + 0] = 255; // R
            rgbaBitmap[i * 4 + 1] = 255; // G
            rgbaBitmap[i * 4 + 2] = 255; // B
            rgbaBitmap[i * 4 + 3] = tempBitmap[i]; // A
        }

        // Create the Direct3D 11 Texture and SRV
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = font.atlasWidth;
        desc.Height = font.atlasHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = rgbaBitmap.data();
        initData.SysMemPitch = font.atlasWidth * 4; // 4 bytes per pixel

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device->CreateTexture2D(&desc, &initData, tex.GetAddressOf()))) throw std::runtime_error("Failed to create Font Texture");
        if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, font.atlasSRV.GetAddressOf()))) throw std::runtime_error("Failed to create Font SRV");

        // Cache and return
        m_fonts[assetID] = font;
        assetManager.SetAssetLoaded(assetID, true);
        return assetID;
    }


    FontAsset* FontManager::GetFont(UUID fontID)
    {
        auto it = m_fonts.find(fontID);
        return it != m_fonts.end() ? &it->second : nullptr;
    }
}