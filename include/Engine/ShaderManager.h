#pragma once
#include <unordered_map>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>
#include "Engine/UUID.h"

// ShaderManager class handles loading, compiling, and binding shaders
// flow of shader loading: Load shaders -> Store in map with shaderID -> Bind when rendering

namespace Engine
{
	class AssetManager;

    class ShaderManager
    {
    public:
		// Compiles & creates VS/PS/InputLayout for BasicVS/PS. Returns a UUID for the shader set.
        UUID LoadBasicShaders(ID3D11Device* device, Engine::AssetManager& assetManager);

		// Compiles SkyboxVS/PS and creates a matching Input Layout. Returns a UUID for the shader set.
        UUID LoadSkyboxShaders(ID3D11Device* device, Engine::AssetManager& assetManager);

        // Compiles UnlitVS/PS and creates a matching Input Layout. Returns a UUID for the shader set.
        UUID LoadUnlitShaders(ID3D11Device* device, Engine::AssetManager& assetManager);

        // Binds shaders & input layout for a shader id
        void Bind(UUID shaderID, ID3D11DeviceContext* context) const;

        // Access input layout for IA
        ID3D11InputLayout* GetInputLayout(UUID shaderID) const;

    private:
		// Internal structure to hold shader data
        struct ShaderData
        {
            Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
            Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
            Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
        };

		// Compiles a shader from file
        static Microsoft::WRL::ComPtr<ID3DBlob> Compile(const std::wstring& path, const std::string& entry, const std::string& target);

        // The master memory pool for shaders
        std::unordered_map<UUID, ShaderData> m_shaders;
    };
}