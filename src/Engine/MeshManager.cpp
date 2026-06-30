#include "Engine/MeshManager.h"
#include <DirectXMath.h>
#include <cmath>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Engine
{
    UUID MeshManager::InitializeCube(ID3D11Device* device, Engine::AssetManager& assetManager)
    {
		// Register the cube mesh in the AssetManager and get its UUID
        UUID assetID = assetManager.ImportAsset("primitive://cube", Engine::AssetType::Mesh);
		// If the cube mesh is already loaded, return its UUID
        if (m_meshes.find(assetID) != m_meshes.end()) return assetID;

        const float s = 0.5f;
        // 24 vertices (4 per face * 6 faces), with positions, normals, and UVs
        std::vector<Vertex> vertices =
        {
            // +Z (front)
            {{-s,-s,+s},{0,0,1},{0,1}}, {{-s,+s,+s},{0,0,1},{0,0}}, {{+s,+s,+s},{0,0,1},{1,0}}, {{+s,-s,+s},{0,0,1},{1,1}},
            // -Z (back)
            {{+s,-s,-s},{0,0,-1},{0,1}}, {{+s,+s,-s},{0,0,-1},{0,0}}, {{-s,+s,-s},{0,0,-1},{1,0}}, {{-s,-s,-s},{0,0,-1},{1,1}},
            // +X (right)
            {{+s,-s,+s},{1,0,0},{0,1}}, {{+s,+s,+s},{1,0,0},{0,0}}, {{+s,+s,-s},{1,0,0},{1,0}}, {{+s,-s,-s},{1,0,0},{1,1}},
            // -X (left)
            {{-s,-s,-s},{-1,0,0},{0,1}}, {{-s,+s,-s},{-1,0,0},{0,0}}, {{-s,+s,+s},{-1,0,0},{1,0}}, {{-s,-s,+s},{-1,0,0},{1,1}},
            // +Y (top)
            {{-s,+s,+s},{0,1,0},{0,1}}, {{-s,+s,-s},{0,1,0},{0,0}}, {{+s,+s,-s},{0,1,0},{1,0}}, {{+s,+s,+s},{0,1,0},{1,1}},
            // -Y (bottom)
            {{-s,-s,-s},{0,-1,0},{0,1}}, {{-s,-s,+s},{0,-1,0},{0,0}}, {{+s,-s,+s},{0,-1,0},{1,0}}, {{+s,-s,-s},{0,-1,0},{1,1}},
        };

        // 36 indices (2 triangles per face * 3 indices per triangle * 6 faces)
        // clockwise winding
        std::vector<uint32_t> indices;
        indices.reserve(6 * 6);
        for (int i = 0; i < 6; ++i)
        {
            uint32_t base = static_cast<uint32_t>(i * 4);
            indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
            indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
        }

		// Create VB/IB through the common path (now always 32-bit index buffers)
        if (CreateMeshBuffers(device, assetID, vertices, indices))
        {
			// Mark the asset as loaded in the AssetManager
            assetManager.SetAssetLoaded(assetID, true);
        }

        return assetID;
    }


    bool MeshManager::CreateMeshBuffers(ID3D11Device* device, UUID assetID,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.empty()) return false;

        // VB
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices.data();

        ComPtr<ID3D11Buffer> vb;
        HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, vb.GetAddressOf());
        if (FAILED(hr)) return false;

        // IB (always 32-bit indices to support large meshes)
        ComPtr<ID3D11Buffer> ib;
        {
            D3D11_BUFFER_DESC ibDesc{};
            ibDesc.Usage = D3D11_USAGE_DEFAULT;
            ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
            ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA ibData{};
            ibData.pSysMem = indices.data();

            hr = device->CreateBuffer(&ibDesc, &ibData, ib.GetAddressOf());
            if (FAILED(hr)) return false;
        }

        MeshData md{};
        md.vb = vb;
        md.ib = ib;
        md.indexCount = static_cast<UINT>(indices.size());
        md.stride = sizeof(Vertex);
        md.idxFmt = DXGI_FORMAT_R32_UINT;   // enforce 32-bit index format

        // CPU-side caches for physics
        md.positions.reserve(vertices.size());
        for (const auto& v : vertices) md.positions.push_back(v.position);
        
        md.indices = indices;

		// Store the MeshData in the map
        m_meshes[assetID] = std::move(md);
        return true;
    }

    std::vector<UUID> MeshManager::LoadModel(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filename)
    {
        // Skip Assimp completely if already parsed
        if (m_modelCache.find(filename) != m_modelCache.end()) {
            return m_modelCache[filename];
        }

		std::vector<UUID> meshIDs;  // to hold the mesh UUIDs for this model
        Assimp::Importer importer;  // create an instance of the Importer class

        // Set import flags
        const unsigned int flags = aiProcess_Triangulate | 
                                    aiProcess_FlipUVs | 
                                    aiProcess_MakeLeftHanded | 
                                    aiProcess_FlipWindingOrder;

        // Read the file and obtain the scene object
        // aiScene is the root object for the imported data
        const aiScene* scene = importer.ReadFile(filename, flags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            std::fprintf(stderr, "Assimp load failed for '%s'\n", filename.c_str());
            return meshIDs;
        }

        // The node object only contains indices to index the actual objects in the scene.
        // The scene contains all the data, node is just to keep stuff organized (like relations between nodes).

        // Process the root node recursively to extract meshes
        ProcessNode(device, assetManager, filename, scene->mRootNode, scene, meshIDs);

        if (meshIDs.empty())
            std::fprintf(stderr, "Assimp: scene loaded but produced no meshes for '%s'\n", filename.c_str());
        else
            m_modelCache[filename] = meshIDs; // Cache for next time

        return meshIDs;
    }


    void MeshManager::ProcessNode(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filename, aiNode* node, const aiScene* scene, std::vector<UUID>& outMeshIDs)
    {
        // Process all meshes at this node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
			// Get the mesh index from the node
            unsigned int meshIndex = node->mMeshes[i];

			// Get the mesh object from the scene
            aiMesh* mesh = scene->mMeshes[meshIndex];

			// Process the mesh and get a UUID for it
            UUID id = ProcessMesh(device, assetManager, filename, mesh, scene, meshIndex);
            if (id != 0) outMeshIDs.push_back(id);
        }

        // Then recurse into children nodes
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            ProcessNode(device, assetManager, filename, node->mChildren[i], scene, outMeshIDs);
        }
    }

    UUID MeshManager::ProcessMesh(ID3D11Device* device, Engine::AssetManager& assetManager, const std::string& filename, aiMesh* mesh, const aiScene* /*scene*/, unsigned int meshIndex)
    {
        // Combine filename and mesh index to create a unique UUID for this sub-mesh
        std::string virtualPath = filename + "?mesh=" + std::to_string(meshIndex);
        UUID assetID = assetManager.ImportAsset(virtualPath, Engine::AssetType::Mesh);

        if (m_meshes.find(assetID) != m_meshes.end()) return assetID;

        std::vector<Vertex> vertices;
        vertices.reserve(mesh->mNumVertices);   // reserve space

        // Extract vertex data
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            Vertex v{};
            // Position
            if (mesh->mVertices) v.position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            else v.position = XMFLOAT3(0, 0, 0);
            // Normal
            if (mesh->mNormals) v.normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            else v.normal = XMFLOAT3(0, 0, 1);   // default normal
            // TexCoord (first channel)
            if (mesh->mTextureCoords[0]) v.texCoord = XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            else v.texCoord = XMFLOAT2(0.0f, 0.0f);

            vertices.push_back(v);
        }

        // Indices (triangulated)
        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
            // Assume triangulated faces
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
            {
                indices.push_back(static_cast<uint32_t>(face.mIndices[j]));
            }
        }

        // Create buffers and store (always 32-bit indices)
        if (CreateMeshBuffers(device, assetID, vertices, indices))
        {
            assetManager.SetAssetLoaded(assetID, true);
        }

        return assetID;
    }


    bool MeshManager::GetMesh(UUID meshID, MeshBuffers& out) const
    {
        auto it = m_meshes.find(meshID);
        if (it == m_meshes.end()) return false;

        const MeshData& md = it->second;
        out.vertexBuffer = md.vb.Get();
        out.indexBuffer = md.ib.Get();
        out.indexCount = md.indexCount;
        out.stride = md.stride;
        out.indexFormat = md.idxFmt;
        return true;
    }


    const std::vector<XMFLOAT3>& MeshManager::GetMeshPositions(UUID meshID) const
    {
        static const std::vector<XMFLOAT3> empty;
        auto it = m_meshes.find(meshID);
        if (it == m_meshes.end()) return empty;
        return it->second.positions;
    }


    const std::vector<uint32_t>& MeshManager::GetMeshIndices(UUID meshID) const
    {
        static const std::vector<uint32_t> empty;
        auto it = m_meshes.find(meshID);
        if (it == m_meshes.end()) return empty;
        return it->second.indices;
    }


    UUID MeshManager::CreateSphere(ID3D11Device* device, Engine::AssetManager& assetManager, float radius, int slices, int stacks)
    {
        std::string virtualPath = "primitive://sphere_" + std::to_string(radius) + "_" + std::to_string(slices) + "_" + std::to_string(stacks);
        UUID assetID = assetManager.ImportAsset(virtualPath, Engine::AssetType::Mesh);
        if (m_meshes.find(assetID) != m_meshes.end()) return assetID;

        if (radius <= 0.0f || slices < 3 || stacks < 2) return 0;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
        indices.reserve(static_cast<size_t>(stacks * slices * 6));

        const float twoPi = 2.0f * XM_PI;
        const float pi = XM_PI;

        for (int i = 0; i <= stacks; ++i)
        {
            float phi = (float)i * (pi / (float)stacks);     // [0..PI]
            float y = radius * cosf(phi);
            float r_xz = radius * sinf(phi);

            for (int j = 0; j <= slices; ++j)
            {
                float theta = (float)j * (twoPi / (float)slices);   // [0..2PI]
                float x = r_xz * cosf(theta);
                float z = r_xz * sinf(theta);

                XMFLOAT3 pos(x, y, z);
                // Normalized position gives normal
                XMVECTOR p = XMLoadFloat3(&pos);
                XMVECTOR n = XMVector3Normalize(p);
                XMFLOAT3 normal{};
                XMStoreFloat3(&normal, n);

                float u = theta / twoPi;
                float v = phi / pi;

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        // CW winding for DirectX LH
        auto idx = [slices](int i, int j) { return static_cast<uint32_t>(i * (slices + 1) + j); };

        for (int i = 0; i < stacks; ++i)
        {
            for (int j = 0; j < slices; ++j)
            {
                uint32_t current = idx(i, j);
                uint32_t next = idx(i + 1, j);
                uint32_t current_next_slice = idx(i, j + 1);
                uint32_t next_stack = idx(i + 1, j + 1);

                // Reference Logic for Winding (Clockwise)
                indices.push_back(current);            indices.push_back(current_next_slice); indices.push_back(next);
                indices.push_back(current_next_slice); indices.push_back(next_stack);         indices.push_back(next);
            }
        }

        if (CreateMeshBuffers(device, assetID, vertices, indices))
        {
            assetManager.SetAssetLoaded(assetID, true);
        }
        return assetID;
    }

    
    UUID MeshManager::CreateCapsule(ID3D11Device* device, Engine::AssetManager& assetManager, float radius, float cylinderHeight, int slices, int stacks)
    {
        std::string virtualPath = "primitive://capsule_" + std::to_string(radius) + "_" + std::to_string(cylinderHeight) + "_" + std::to_string(slices) + "_" + std::to_string(stacks);
        UUID assetID = assetManager.ImportAsset(virtualPath, Engine::AssetType::Mesh);
        if (m_meshes.find(assetID) != m_meshes.end()) return assetID;

        if (radius <= 0.0f || cylinderHeight < 0.0f || slices < 3 || stacks < 2) return 0;

        // Build full sphere param but offset Y for hemispheres and duplicate equator ring
        // Hemisphere stacks: split stacks into top/bottom halves
        int hemiStacks = stacks;    // resolution for each hemisphere
        int cylStacks = 1;          // one ring strip for cylinder wall (can increase for smoother mapping)

        const float twoPi = 2.0f * XM_PI;
        const float pi = XM_PI;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Generate top hemisphere (phi: 0..PI/2)
        for (int i = 0; i <= hemiStacks; ++i)
        {
            float phi = (float)i * (pi * 0.5f / (float)hemiStacks); // [0..PI/2]
            float y = radius * cosf(phi) + cylinderHeight * 0.5f;   // shifted up by +H/2
            float r_xz = radius * sinf(phi);

            for (int j = 0; j <= slices; ++j)
            {
                float theta = (float)j * (twoPi / (float)slices);
                float x = r_xz * cosf(theta);
                float z = r_xz * sinf(theta);

                XMFLOAT3 pos(x, y, z);
                XMVECTOR n = XMVector3Normalize(XMVectorSet(x, y - cylinderHeight * 0.5f, z, 0.0f));
                XMFLOAT3 normal{}; XMStoreFloat3(&normal, n);

                float u = theta / twoPi;
                float v = (phi) / pi;   // map top hemi v [0..0.5]

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        int topEquatorStart = (int)vertices.size() - (slices + 1);
        for (int c = 0; c < cylStacks; ++c)
        {
            float y = cylinderHeight * 0.5f - (float)(c + 1) * (cylinderHeight / (float)cylStacks);
            for (int j = 0; j <= slices; ++j)
            {
                const Vertex& eqV = vertices[topEquatorStart + j];
                XMFLOAT3 pos(eqV.position.x, y, eqV.position.z);
                XMVECTOR n = XMVector3Normalize(XMVectorSet(pos.x, 0.0f, pos.z, 0.0f));
                XMFLOAT3 normal{}; XMStoreFloat3(&normal, n);

                float u = eqV.texCoord.x;
                float v = 0.5f;

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        for (int i = 0; i <= hemiStacks; ++i)
        {
            float phi = (float)i * (pi * 0.5f / (float)hemiStacks) + pi * 0.5f;
            float y = radius * cosf(phi) - cylinderHeight * 0.5f;
            float r_xz = radius * sinf(phi);

            for (int j = 0; j <= slices; ++j)
            {
                float theta = (float)j * (twoPi / (float)slices);
                float x = r_xz * cosf(theta);
                float z = r_xz * sinf(theta);

                XMFLOAT3 pos(x, y, z);
                XMVECTOR n = XMVector3Normalize(XMVectorSet(x, y + cylinderHeight * 0.5f, z, 0.0f));
                XMFLOAT3 normal{}; XMStoreFloat3(&normal, n);

                float u = theta / twoPi;
                float v = phi / pi;

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        // Build indices across the three segments: top hemi, cylinder strip(s), bottom hemi
        auto idx = [slices](int row, int col) { return static_cast<uint32_t>(row * (slices + 1) + col); };
        // Total rows in vertex grid progression
        // Created vertices in order: rowsTopHemi, then rowsCylinder, then rowsBottomHemi
        int totalRows = (hemiStacks + 1) + cylStacks + (hemiStacks + 1);

        // Generate strip triangles for each adjacent row pair
        // CW winding consistent with reference
        for (int row = 0; row < totalRows - 1; ++row)
        {
            // Each row has (slices + 1) verts
            int rowA = row;
            int rowB = row + 1;

            for (int j = 0; j < slices; ++j)
            {
                uint32_t a = idx(rowA, j);
                uint32_t b = idx(rowB, j);
                uint32_t a1 = idx(rowA, j + 1);
                uint32_t b1 = idx(rowB, j + 1);

                // CW
                indices.push_back(a);  indices.push_back(a1); indices.push_back(b);
                indices.push_back(a1); indices.push_back(b1); indices.push_back(b);
            }
        }

        if (CreateMeshBuffers(device, assetID, vertices, indices))
        {
            assetManager.SetAssetLoaded(assetID, true);
        }
        return assetID;
    }
}