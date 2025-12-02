#pragma once
#include <unordered_map>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>

namespace Engine
{
    class ShaderManager
    {
    public:
		// Compiles & creates VS/PS/InputLayout for BasicVS/PS. Returns shaderID 1. (temporary ID)
        int LoadBasicShaders(ID3D11Device* device);

        // Binds shaders & input layout for a shader id
        void Bind(int shaderID, ID3D11DeviceContext* context) const;

        // Access input layout for IA
        ID3D11InputLayout* GetInputLayout(int shaderID) const;

    private:
        struct ShaderData
        {
            Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
            Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
            Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
        };

        static Microsoft::WRL::ComPtr<ID3DBlob> Compile(const std::wstring& path, const std::string& entry, const std::string& target);

        std::unordered_map<int, ShaderData> m_shaders;
    };
}