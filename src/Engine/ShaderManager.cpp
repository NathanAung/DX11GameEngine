#include "Engine/ShaderManager.h"
#include "Engine/MeshManager.h" // For Vertex layout
#include <d3dcompiler.h>

using Microsoft::WRL::ComPtr;

namespace Engine
{
    ComPtr<ID3DBlob> ShaderManager::Compile(const std::wstring& path, const std::string& entry, const std::string& target)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
    #if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
    #endif
        ComPtr<ID3DBlob> bytecode, errors;
        HRESULT hr = D3DCompileFromFile(
            path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry.c_str(),
            target.c_str(),
            flags,
            0,
            bytecode.GetAddressOf(),
            errors.GetAddressOf());

        if (FAILED(hr))
        {
            if (errors) OutputDebugStringA((const char*)errors->GetBufferPointer());
            throw std::runtime_error("Failed to compile shader");
        }
        return bytecode;
    }


    int ShaderManager::LoadBasicShaders(ID3D11Device* device)
    {
        ShaderData sd{};

        auto vsByte = Compile(L"shaders/BasicVS.hlsl", "main", "vs_5_0");
        auto psByte = Compile(L"shaders/BasicPS.hlsl", "main", "ps_5_0");

        if (FAILED(device->CreateVertexShader(vsByte->GetBufferPointer(), vsByte->GetBufferSize(), nullptr, sd.vs.GetAddressOf())))
            return -1;
        if (FAILED(device->CreatePixelShader(psByte->GetBufferPointer(), psByte->GetBufferSize(), nullptr, sd.ps.GetAddressOf())))
            return -1;

        // Input layout (matches Engine::Vertex: POSITION, NORMAL, TEXCOORD)
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Engine::Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Engine::Vertex, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Engine::Vertex, texCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        if (FAILED(device->CreateInputLayout(layout, _countof(layout), vsByte->GetBufferPointer(), vsByte->GetBufferSize(), sd.inputLayout.GetAddressOf())))
            return -1;

		m_shaders.emplace(1, std::move(sd));    // store as shader ID 1 (temporary)
		return 1;   // return shader ID 1 (temporary)
    }


    void ShaderManager::Bind(int shaderID, ID3D11DeviceContext* context) const
    {
		// iterator to shader map
        auto it = m_shaders.find(shaderID);
        if (it == m_shaders.end()) return;

		// Bind shaders and input layout
		// second is used becase map value is pair<key, value>
        const ShaderData& sd = it->second;
        context->VSSetShader(sd.vs.Get(), nullptr, 0);
        context->PSSetShader(sd.ps.Get(), nullptr, 0);
        context->IASetInputLayout(sd.inputLayout.Get());
    }


    ID3D11InputLayout* ShaderManager::GetInputLayout(int shaderID) const
    {
        auto it = m_shaders.find(shaderID);
        if (it == m_shaders.end()) return nullptr;
        return it->second.inputLayout.Get();
    }
}