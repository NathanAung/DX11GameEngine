#include "Engine/ShaderManager.h"
#include <d3dcompiler.h>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace Engine
{
    // Helper compile function (already declared in header)
    ComPtr<ID3DBlob> ShaderManager::Compile(const std::wstring& path, const std::string& entry, const std::string& target)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    #if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #endif

        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3DBlob> errors;
        HRESULT hr = D3DCompileFromFile(
            path.c_str(),
            nullptr, nullptr,
            entry.c_str(), target.c_str(),
            flags, 0,
            bytecode.GetAddressOf(),
            errors.GetAddressOf());

        if (FAILED(hr))
        {
            const char* msg = errors ? (const char*)errors->GetBufferPointer() : "Unknown shader compile error";
            throw std::runtime_error(msg);
        }
        return bytecode;
    }

    int ShaderManager::LoadBasicShaders(ID3D11Device* device)
    {
        // Compile shaders
        ComPtr<ID3DBlob> vsBytecode = Compile(L"shaders/BasicVS.hlsl", "main", "vs_5_0");
        ComPtr<ID3DBlob> psBytecode = Compile(L"shaders/BasicPS.hlsl", "main", "ps_5_0");

        // Create shader objects
        ShaderData sd{};
        HRESULT hr = device->CreateVertexShader(vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(), nullptr, sd.vs.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed (Basic)");
        hr = device->CreatePixelShader(psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(), nullptr, sd.ps.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed (Basic)");

        // Input layout must match Engine::Vertex (Position, Normal, TexCoord) with stride 32
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,                         D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  sizeof(float)*3,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  sizeof(float)*6,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = device->CreateInputLayout(
            layout, _countof(layout),
            vsBytecode->GetBufferPointer(),
            vsBytecode->GetBufferSize(),
            sd.inputLayout.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed (Basic)");

        // Store under shaderID 1
        m_shaders[1] = std::move(sd);
        return 1;
    }

    int ShaderManager::LoadSkyboxShaders(ID3D11Device* device)
    {
        // Compile Skybox VS/PS
        ComPtr<ID3DBlob> vsBytecode = Compile(L"shaders/SkyboxVS.hlsl", "main", "vs_5_0");
        ComPtr<ID3DBlob> psBytecode = Compile(L"shaders/SkyboxPS.hlsl", "main", "ps_5_0");

        // Create shader objects
        ShaderData sd{};
        HRESULT hr = device->CreateVertexShader(vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(), nullptr, sd.vs.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed (Skybox)");
        hr = device->CreatePixelShader(psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(), nullptr, sd.ps.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed (Skybox)");

        // IMPORTANT: Use the exact same input layout as LoadBasicShaders
        // Reason: We render the skybox with the standard cube mesh (Engine::Vertex: POSITION, NORMAL, TEXCOORD)
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,                         D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  sizeof(float)*3,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  sizeof(float)*6,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = device->CreateInputLayout(
            layout, _countof(layout),
            vsBytecode->GetBufferPointer(),
            vsBytecode->GetBufferSize(),
            sd.inputLayout.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed (Skybox)");

        // Store under shaderID 2
        m_shaders[2] = std::move(sd);
        return 2;
    }

    void ShaderManager::Bind(int shaderID, ID3D11DeviceContext* context) const
    {
        auto it = m_shaders.find(shaderID);
        if (it == m_shaders.end()) return;

        const ShaderData& sd = it->second;
        if (sd.vs) context->VSSetShader(sd.vs.Get(), nullptr, 0);
        if (sd.ps) context->PSSetShader(sd.ps.Get(), nullptr, 0);
        if (sd.inputLayout) context->IASetInputLayout(sd.inputLayout.Get());
    }

    ID3D11InputLayout* ShaderManager::GetInputLayout(int shaderID) const
    {
        auto it = m_shaders.find(shaderID);
        if (it == m_shaders.end()) return nullptr;
        return it->second.inputLayout.Get();
    }
}