#pragma once
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/UUID.h"
#include "Engine/AssetManager.h"

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
        DXGI_FORMAT   indexFormat  = DXGI_FORMAT_R32_UINT;
    };

    class MeshManager
    {
    public:
        // Procedural primitives
        // Creates a unit cube mesh
        UUID InitializeCube(ID3D11Device* device, Engine::AssetManager& assetManager);
        UUID CreateSphere(ID3D11Device* device, Engine::AssetManager& assetManager, float radius, int slices, int stacks);
        // Capsule (Y-axis aligned). radius = sphere radius, cylinderHeight = straight section height (no caps).
        UUID CreateCapsule(ID3D11Device* device, Engine::AssetManager& assetManager, float radius, float cylinderHeight, int slices, int stacks);

        // Loads a model with Assimp and returns mesh UUIDs for all mesh parts
        std::vector<UUID> LoadModel(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filename);

        // Retrieves buffers for a mesh ID
        bool GetMesh(UUID meshID, MeshBuffers& out) const;

        // Accessors for physics
        const std::vector<DirectX::XMFLOAT3>& GetMeshPositions(UUID meshID) const;
        const std::vector<uint32_t>& GetMeshIndices(UUID meshID) const;

    private:
        // Internal structure to hold mesh data
        struct MeshData
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
            Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
            UINT indexCount = 0;
            UINT stride     = 0;
            DXGI_FORMAT idxFmt = DXGI_FORMAT_R32_UINT;  // default to 32-bit indices

            // CPU-side cached data for physics
            std::vector<DirectX::XMFLOAT3> positions;  // vertex positions
            std::vector<uint32_t> indices;             // triangle indices
        };

		// Creates vertex and index buffers for a mesh and stores them in the mesh map
        bool CreateMeshBuffers(ID3D11Device* device,
                              UUID assetID,
                              const std::vector<Vertex>& vertices,
                              const std::vector<uint32_t>& indices);

        // Process Assimp node recursively
        void ProcessNode(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filename, aiNode* node, const aiScene* scene, std::vector<UUID>& outMeshIDs);

        // Convert Assimp mesh to internal buffers, store and return UUID
        UUID ProcessMesh(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filename, aiMesh* mesh, const aiScene* scene, unsigned int meshIndex);

        // The master memory pool for meshes
        std::unordered_map<UUID, MeshData> m_meshes;

        // Cache to prevent Assimp from re-parsing the same file multiple times
        std::unordered_map<std::string, std::vector<UUID>> m_modelCache;
    };
}