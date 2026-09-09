#include "Engine/ShaderManager.h"
#include <d3dcompiler.h>
#include <stdexcept>
#include <fstream>
#include "Engine/AssetManager.h"

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


    UUID ShaderManager::LoadBasicShaders(ID3D11Device* device, Engine::AssetManager& assetManager)
    {
		// Register the shader set in the AssetManager and get its UUID
        UUID assetID = assetManager.ImportAsset("shader://basic", Engine::AssetType::Shader);
        if (m_shaders.find(assetID) != m_shaders.end()) return assetID;

        //// Compile shaders
        //ComPtr<ID3DBlob> vsBytecode = Compile(L"shaders/BasicVS.hlsl", "main", "vs_5_0");
        //ComPtr<ID3DBlob> psBytecode = Compile(L"shaders/BasicPS.hlsl", "main", "ps_5_0");

        // Route through the binary bytecode handler
        std::vector<char> vsBytecode = GetShaderBytecode(assetManager, L"shaders/BasicVS.hlsl", "shaders/BasicVS.cso", "main", "vs_5_0");
        std::vector<char> psBytecode = GetShaderBytecode(assetManager, L"shaders/BasicPS.hlsl", "shaders/BasicPS.cso", "main", "ps_5_0");

        // Create shader objects
        ShaderData sd{};
        HRESULT hr = device->CreateVertexShader(vsBytecode.data(), vsBytecode.size(), nullptr, sd.vs.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed (Basic)");
        hr = device->CreatePixelShader(psBytecode.data(), psBytecode.size(), nullptr, sd.ps.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed (Basic)");

        // Input layout must match Engine::Vertex (Position, Normal, TexCoord) with stride 32
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,                         D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  sizeof(float) * 3,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  sizeof(float) * 6,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = device->CreateInputLayout(
            layout, _countof(layout),
            vsBytecode.data(),
            vsBytecode.size(),
            sd.inputLayout.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed (Basic)");

        m_shaders[assetID] = std::move(sd);
        assetManager.SetAssetLoaded(assetID, true);
        return assetID;
    }


    UUID ShaderManager::LoadSkyboxShaders(ID3D11Device* device, Engine::AssetManager& assetManager)
    {
		// Register the shader set in the AssetManager and get its UUID
        UUID assetID = assetManager.ImportAsset("shader://skybox", Engine::AssetType::Shader);
        if (m_shaders.find(assetID) != m_shaders.end()) return assetID;

        //// Compile Skybox VS/PS
        //ComPtr<ID3DBlob> vsBytecode = Compile(L"shaders/SkyboxVS.hlsl", "main", "vs_5_0");
        //ComPtr<ID3DBlob> psBytecode = Compile(L"shaders/SkyboxPS.hlsl", "main", "ps_5_0");

		// Route through the binary bytecode handler
        std::vector<char> vsBytecode = GetShaderBytecode(assetManager, L"shaders/SkyboxVS.hlsl", "shaders/SkyboxVS.cso", "main", "vs_5_0");
        std::vector<char> psBytecode = GetShaderBytecode(assetManager, L"shaders/SkyboxPS.hlsl", "shaders/SkyboxPS.cso", "main", "ps_5_0");

        // Create shader objects
        ShaderData sd{};
        HRESULT hr = device->CreateVertexShader(vsBytecode.data(), vsBytecode.size(), nullptr, sd.vs.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed (Skybox)");
        hr = device->CreatePixelShader(psBytecode.data(), psBytecode.size(), nullptr, sd.ps.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed (Skybox)");

        // IMPORTANT: Use the exact same input layout as LoadBasicShaders
        // Reason: We render the skybox with the standard cube mesh (Engine::Vertex: POSITION, NORMAL, TEXCOORD)
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,                         D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  sizeof(float) * 3,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  sizeof(float) * 6,           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = device->CreateInputLayout(
            layout, _countof(layout),
            vsBytecode.data(),
            vsBytecode.size(),
            sd.inputLayout.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed (Skybox)");

        m_shaders[assetID] = std::move(sd);
        assetManager.SetAssetLoaded(assetID, true);
        return assetID;
    }


    UUID ShaderManager::LoadUnlitShaders(ID3D11Device* device, Engine::AssetManager& assetManager)
    {
		// Register the shader set in the AssetManager and get its UUID
        UUID assetID = assetManager.ImportAsset("shader://unlit", Engine::AssetType::Shader);
        if (m_shaders.find(assetID) != m_shaders.end()) return assetID;

		//// Compile Unlit VS/PS
        //Microsoft::WRL::ComPtr<ID3DBlob> vsBytecode = Compile(L"shaders/UnlitVS.hlsl", "main", "vs_5_0");
        //Microsoft::WRL::ComPtr<ID3DBlob> psBytecode = Compile(L"shaders/UnlitPS.hlsl", "main", "ps_5_0");

        // Route through the binary bytecode handler
        std::vector<char> vsBytecode = GetShaderBytecode(assetManager, L"shaders/UnlitVS.hlsl", "shaders/UnlitVS.cso", "main", "vs_5_0");
        std::vector<char> psBytecode = GetShaderBytecode(assetManager, L"shaders/UnlitPS.hlsl", "shaders/UnlitPS.cso", "main", "ps_5_0");

		// Create shader objects
        ShaderData sd;
        HRESULT hr = device->CreateVertexShader(vsBytecode.data(), vsBytecode.size(), nullptr, sd.vs.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed (Unlit)");

        hr = device->CreatePixelShader(psBytecode.data(), psBytecode.size(), nullptr, sd.ps.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed (Unlit)");

		// Input layout must match Engine::Vertex (Position, Normal, TexCoord) with stride 32
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,               D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  sizeof(float) * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = device->CreateInputLayout(layout, _countof(layout), vsBytecode.data(), vsBytecode.size(), sd.inputLayout.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed (Unlit)");

        m_shaders[assetID] = std::move(sd);
        assetManager.SetAssetLoaded(assetID, true);
        return assetID;
    }


    void ShaderManager::Bind(UUID shaderID, ID3D11DeviceContext* context) const
    {
        auto it = m_shaders.find(shaderID);
        if (it == m_shaders.end()) return;

        const ShaderData& sd = it->second;
        if (sd.vs) context->VSSetShader(sd.vs.Get(), nullptr, 0);
        if (sd.ps) context->PSSetShader(sd.ps.Get(), nullptr, 0);
        if (sd.inputLayout) context->IASetInputLayout(sd.inputLayout.Get());
    }


    ID3D11InputLayout* ShaderManager::GetInputLayout(UUID shaderID) const
    {
        auto it = m_shaders.find(shaderID);
        if (it == m_shaders.end()) return nullptr;
        return it->second.inputLayout.Get();
    }


    std::vector<char> ShaderManager::GetShaderBytecode(Engine::AssetManager& assetManager, const std::wstring& hlslPath, const std::string& csoPath, const std::string& entry, const std::string& target)
    {
        // Register the binary CSO file so the AssetManager packs it into the archive
        Engine::UUID csoUUID = assetManager.ImportAsset(csoPath, Engine::AssetType::Shader);

        if (assetManager.IsVFSActive())
        {
            // GAME MODE: Read the pre-compiled binary blob directly from the VFS archive
            std::vector<char> buffer = assetManager.ReadAssetFromVFS(csoUUID);
            if (buffer.empty()) throw std::runtime_error("Failed to load CSO from VFS: " + csoPath);
            return buffer;
        }
        else
        {
            // EDITOR MODE: Compile HLSL text to Bytecode
            ComPtr<ID3DBlob> blob = Compile(hlslPath, entry, target);

            std::vector<char> buffer(blob->GetBufferSize());
            std::memcpy(buffer.data(), blob->GetBufferPointer(), blob->GetBufferSize());

            // Save the compiled bytecode to a physical .cso file on disk for the Packer
            std::ofstream outFile(csoPath, std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(buffer.data(), buffer.size());
                outFile.close();
            }

            return buffer;
        }
    }
}