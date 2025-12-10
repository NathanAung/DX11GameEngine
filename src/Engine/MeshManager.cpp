#include "Engine/MeshManager.h"
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Engine
{
    int MeshManager::InitializeCube(ID3D11Device* device)
    {
        // Increase cube size for skybox stability (avoid near-plane clipping before VS z=w trick)
        const float s = 2.0f;
        std::vector<Vertex> vertices =
        {
            {{-s,-s,-s},{0,0,-1},{0,0}}, {{-s,+s,-s},{0,0,-1},{0,0}}, {{+s,+s,-s},{0,0,-1},{0,0}}, {{+s,-s,-s},{0,0,-1},{0,0}}, // back
            {{-s,-s,+s},{0,0,-1},{0,0}}, {{-s,+s,+s},{0,0,-1},{0,0}}, {{+s,+s,+s},{0,0,-1},{0,0}}, {{+s,-s,+s},{0,0,-1},{0,0}}  // front
        };
        std::vector<uint16_t> indices16 =
        {
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            4, 5, 1, 4, 1, 0,
            3, 2, 6, 3, 6, 7,
            1, 5, 6, 1, 6, 2,
            4, 0, 3, 4, 3, 7
        };

        // VB
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices.data();

        ComPtr<ID3D11Buffer> vb;
        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, vb.GetAddressOf())))
            return -1;

        // IB
        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.ByteWidth = static_cast<UINT>(indices16.size() * sizeof(uint16_t));
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = indices16.data();

        ComPtr<ID3D11Buffer> ib;
        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, ib.GetAddressOf())))
            return -1;

		// Store MeshData
        MeshData md{};
        md.vb = vb;
        md.ib = ib;
        md.indexCount = static_cast<UINT>(indices16.size());
        md.stride = sizeof(Vertex);
        md.idxFmt = DXGI_FORMAT_R16_UINT;

        m_meshes.emplace(101, std::move(md));   // store as mesh ID 101 (temporary)
        return 101; // temporary mesh ID
    }


    // Helper: Creates VB/IB for given data, stores MeshData, returns new ID
    int MeshManager::CreateMeshBuffers(ID3D11Device* device,
                                       const std::vector<Vertex>& vertices,
                                       const std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.empty())
            return -1;

        // VB
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices.data();

        ComPtr<ID3D11Buffer> vb;
        HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, vb.GetAddressOf());
        if (FAILED(hr)) return -1;

        // Choose 16-bit indices if possible; fallback to 32-bit otherwise
        bool use16 = (indices.size() < 65536);
        DXGI_FORMAT idxFmt = use16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		// IB
        ComPtr<ID3D11Buffer> ib;
        if (use16)
        {
            std::vector<uint16_t> idx16(indices.begin(), indices.end());
            D3D11_BUFFER_DESC ibDesc{};
            ibDesc.Usage = D3D11_USAGE_DEFAULT;
            ibDesc.ByteWidth = static_cast<UINT>(idx16.size() * sizeof(uint16_t));
            ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA ibData{};
            ibData.pSysMem = idx16.data();

            hr = device->CreateBuffer(&ibDesc, &ibData, ib.GetAddressOf());
            if (FAILED(hr)) return -1;
        }
        else
        {
            D3D11_BUFFER_DESC ibDesc{};
            ibDesc.Usage = D3D11_USAGE_DEFAULT;
            ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
            ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA ibData{};
            ibData.pSysMem = indices.data();

            hr = device->CreateBuffer(&ibDesc, &ibData, ib.GetAddressOf());
            if (FAILED(hr)) return -1;
        }

        MeshData md{};
        md.vb = vb;
        md.ib = ib;
        md.indexCount = static_cast<UINT>(indices.size());
        md.stride = sizeof(Vertex);
        md.idxFmt = idxFmt;

        const int id = m_nextMeshID++;
        m_meshes.emplace(id, std::move(md));
        return id;
    }


    std::vector<int> MeshManager::LoadModel(ID3D11Device* device, const std::string& filename)
    {
        std::vector<int> meshIDs;

		Assimp::Importer importer;  // create an instance of the Importer class

		// Set import flags
        const unsigned int flags =
            aiProcess_Triangulate |
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
        ProcessNode(device, scene->mRootNode, scene, meshIDs);
        if (meshIDs.empty())
            std::fprintf(stderr, "Assimp: scene loaded but produced no meshes for '%s'\n", filename.c_str());

        return meshIDs;
    }


    void MeshManager::ProcessNode(ID3D11Device* device, aiNode* node, const aiScene* scene, std::vector<int>& outMeshIDs)
    {
        // Process all meshes at this node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {   
			// Get the mesh object from the scene
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            int id = ProcessMesh(device, mesh, scene);
            if (id != -1)
                outMeshIDs.push_back(id);
        }

		// Then recurse into children nodes
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            ProcessNode(device, node->mChildren[i], scene, outMeshIDs);
        }
    }


    int MeshManager::ProcessMesh(ID3D11Device* device, aiMesh* mesh, const aiScene* /*scene*/)
    {
        std::vector<Vertex> vertices;
		vertices.reserve(mesh->mNumVertices);   // reserve space

        // Extract vertex data
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            Vertex v{};
            // Position
            if (mesh->mVertices)
            {
                v.position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            }
            else
            {
                v.position = XMFLOAT3(0, 0, 0);
            }
            // Normal
            if (mesh->mNormals)
            {
                v.normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            else
            {
                v.normal = XMFLOAT3(0, 0, 1); // default normal
            }
            // TexCoord (first channel)
            if (mesh->mTextureCoords[0])
            {
                v.texCoord = XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }
            else
            {
                v.texCoord = XMFLOAT2(0.0f, 0.0f);
            }

            vertices.push_back(v);
        }

        // Indices
        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3); // triangulated
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
			// Assume triangulated faces
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
            {
                indices.push_back(static_cast<uint32_t>(face.mIndices[j]));
            }
        }

        // Create buffers and store
        return CreateMeshBuffers(device, vertices, indices);
    }


    bool MeshManager::GetMesh(int meshID, MeshBuffers& out) const
    {
        auto it = m_meshes.find(meshID);
        if (it == m_meshes.end()) return false;

        const MeshData& md = it->second;
        out.vertexBuffer = md.vb.Get();
        out.indexBuffer  = md.ib.Get();
        out.indexCount   = md.indexCount;
        out.stride       = md.stride;
        out.indexFormat  = md.idxFmt;
        return true;
    }
}