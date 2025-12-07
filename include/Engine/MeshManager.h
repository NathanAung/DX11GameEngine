#pragma once
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

// Assimp - model importing
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// MeshManager class handles creation and storage of mesh buffers
// Flow of model loading: LoadModel() -> ProcessNode() -> ProcessMesh() -> CreateMeshBuffers()

namespace Engine
{
    // Vertex format used by BasicVS.hlsl
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;    // per-vertex normal
        DirectX::XMFLOAT2 texCoord;  // per-vertex UV
    };

    // Structure to hold mesh buffers
    struct MeshBuffers
    {
        ID3D11Buffer* vertexBuffer = nullptr;
        ID3D11Buffer* indexBuffer  = nullptr;
        UINT          indexCount   = 0;
        UINT          stride       = 0;
        DXGI_FORMAT   indexFormat  = DXGI_FORMAT_R16_UINT;
    };

    class MeshManager
    {
    public:
        // Creates a unit cube mesh and stores it as meshID 101. Returns 101. (temporary ID)
        int InitializeCube(ID3D11Device* device);

        // Loads a model with Assimp and returns mesh IDs for all mesh parts
        std::vector<int> LoadModel(ID3D11Device* device, const std::string& filename);

        // Retrieves buffers for a mesh ID
        bool GetMesh(int meshID, MeshBuffers& out) const;

    private:
        // Internal structure to hold mesh data
        struct MeshData
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
            Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
            UINT indexCount = 0;
            UINT stride     = 0;
            DXGI_FORMAT idxFmt = DXGI_FORMAT_R16_UINT;
        };

        // Create buffers and store MeshData; returns assigned mesh ID
        int CreateMeshBuffers(ID3D11Device* device,
                              const std::vector<Vertex>& vertices,
                              const std::vector<uint32_t>& indices);

        // Process Assimp node recursively
        void ProcessNode(ID3D11Device* device, aiNode* node, const aiScene* scene, std::vector<int>& outMeshIDs);

        // Convert Assimp mesh to internal buffers, store and return ID
        int ProcessMesh(ID3D11Device* device, aiMesh* mesh, const aiScene* scene);

        // Map of meshID to MeshData
        std::unordered_map<int, MeshData> m_meshes;

        // ID allocator (starts after reserved 101)
        int m_nextMeshID = 102;
    };
}