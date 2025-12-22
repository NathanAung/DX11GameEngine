#include "Engine/MeshManager.h"
#include <DirectXMath.h>
#include <cmath>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Engine
{
    int MeshManager::InitializeCube(ID3D11Device* device)
    {
        const float s = 0.5f;
		// 24 vertices (4 per face * 6 faces), with positions, normals, and UVs
        std::vector<Vertex> vertices =
        {
            // +Z (front)
            {{-s,-s,+s},{0,0,1},{0,1}},
            {{-s,+s,+s},{0,0,1},{0,0}},
            {{+s,+s,+s},{0,0,1},{1,0}},
            {{+s,-s,+s},{0,0,1},{1,1}},

            // -Z (back)
            {{+s,-s,-s},{0,0,-1},{0,1}},
            {{+s,+s,-s},{0,0,-1},{0,0}},
            {{-s,+s,-s},{0,0,-1},{1,0}},
            {{-s,-s,-s},{0,0,-1},{1,1}},

            // +X (right)
            {{+s,-s,+s},{1,0,0},{0,1}},
            {{+s,+s,+s},{1,0,0},{0,0}},
            {{+s,+s,-s},{1,0,0},{1,0}},
            {{+s,-s,-s},{1,0,0},{1,1}},

            // -X (left)
            {{-s,-s,-s},{-1,0,0},{0,1}},
            {{-s,+s,-s},{-1,0,0},{0,0}},
            {{-s,+s,+s},{-1,0,0},{1,0}},
            {{-s,-s,+s},{-1,0,0},{1,1}},

            // +Y (top)
            {{-s,+s,+s},{0,1,0},{0,1}},
            {{-s,+s,-s},{0,1,0},{0,0}},
            {{+s,+s,-s},{0,1,0},{1,0}},
            {{+s,+s,+s},{0,1,0},{1,1}},

            // -Y (bottom)
            {{-s,-s,-s},{0,-1,0},{0,1}},
            {{-s,-s,+s},{0,-1,0},{0,0}},
            {{+s,-s,+s},{0,-1,0},{1,0}},
            {{+s,-s,-s},{0,-1,0},{1,1}},
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
        return CreateMeshBuffersWithID(device, 101, vertices, indices);
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
            if (FAILED(hr)) return -1;
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

        const int id = m_nextMeshID++;
        m_meshes.emplace(id, std::move(md));
        return id;
    }

    int MeshManager::CreateMeshBuffersWithID(
        ID3D11Device* device,
        int forcedID,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.empty())
            return -1;

        // Prevent accidental overwrite
		if (m_meshes.find(forcedID) != m_meshes.end())
            return -1;

        // --- VB ---
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = UINT(vertices.size() * sizeof(Vertex));
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices.data();

        ComPtr<ID3D11Buffer> vb;
        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, vb.GetAddressOf())))
            return -1;

        // --- IB ---
        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.ByteWidth = UINT(indices.size() * sizeof(uint32_t));
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = indices.data();

        ComPtr<ID3D11Buffer> ib;
        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, ib.GetAddressOf())))
            return -1;

        MeshData md{};
        md.vb = vb;
        md.ib = ib;
        md.indexCount = UINT(indices.size());
        md.stride = sizeof(Vertex);
        md.idxFmt = DXGI_FORMAT_R32_UINT;

        md.positions.reserve(vertices.size());
        for (const auto& v : vertices)
            md.positions.push_back(v.position);

        md.indices = indices;

        m_meshes.emplace(forcedID, std::move(md));

        // Keep auto IDs from colliding later
        m_nextMeshID = std::max(m_nextMeshID, forcedID + 1);

        return forcedID;
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


    const std::vector<XMFLOAT3>& MeshManager::GetMeshPositions(int meshID) const
    {
        static const std::vector<XMFLOAT3> empty;
        auto it = m_meshes.find(meshID);
        if (it == m_meshes.end()) return empty;
        return it->second.positions;
    }


    const std::vector<uint32_t>& MeshManager::GetMeshIndices(int meshID) const
    {
        static const std::vector<uint32_t> empty;
        auto it = m_meshes.find(meshID);
        if (it == m_meshes.end()) return empty;
        return it->second.indices;
    }


    int MeshManager::CreateSphere(ID3D11Device* device, float radius, int slices, int stacks)
    {
        if (radius <= 0.0f || slices < 3 || stacks < 2) return -1;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
        indices.reserve(static_cast<size_t>(stacks * slices * 6));

        const float twoPi = 2.0f * XM_PI;
        const float pi = XM_PI;

        for (int i = 0; i <= stacks; ++i)
        {
            float phi = (float)i * (pi / (float)stacks); // [0..PI]
            float y = radius * cosf(phi);
            float r_xz = radius * sinf(phi);

            for (int j = 0; j <= slices; ++j)
            {
                float theta = (float)j * (twoPi / (float)slices); // [0..2PI]
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

        return CreateMeshBuffers(device, vertices, indices);
    }

    
    int MeshManager::CreateCapsule(ID3D11Device* device, float radius, float cylinderHeight, int slices, int stacks)
    {
        if (radius <= 0.0f || cylinderHeight < 0.0f || slices < 3 || stacks < 2) return -1;

        // Build full sphere param but offset Y for hemispheres and duplicate equator ring
        // Hemisphere stacks: split stacks into top/bottom halves
        int hemiStacks = stacks;          // resolution for each hemisphere
        int cylStacks = 1;                // one ring strip for cylinder wall (can increase for smoother mapping)

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
                XMVECTOR p = XMLoadFloat3(&pos);
                XMVECTOR n = XMVector3Normalize(XMVectorSet(x, y - cylinderHeight * 0.5f, z, 0.0f)); // normal from local sphere center
                XMFLOAT3 normal{}; XMStoreFloat3(&normal, n);

                float u = theta / twoPi;
                float v = (phi) / pi; // map top hemi v [0..0.5]

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        // Duplicate equator ring (phi = PI/2) to start cylinder (for proper UV continuity)
        int topEquatorStart = (int)vertices.size() - (slices + 1);

        for (int c = 0; c < cylStacks; ++c)
        {
            float y = cylinderHeight * 0.5f - (float)(c + 1) * (cylinderHeight / (float)cylStacks); // from +H/2 down to -H/2
            for (int j = 0; j <= slices; ++j)
            {
                const Vertex& eqV = vertices[topEquatorStart + j];
                XMFLOAT3 pos(eqV.position.x, y, eqV.position.z);
                // Cylinder normal points radially in XZ
                XMVECTOR n = XMVector3Normalize(XMVectorSet(pos.x, 0.0f, pos.z, 0.0f));
                XMFLOAT3 normal{}; XMStoreFloat3(&normal, n);

                float u = eqV.texCoord.x;
                // Stretch v across cylinder [0.5 .. 0.5] (can map more nicely if desired). Keep v = 0.5 here.
                float v = 0.5f;

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        // Generate bottom hemisphere (phi: PI/2..PI)
        for (int i = 0; i <= hemiStacks; ++i)
        {
            float phi = (float)i * (pi * 0.5f / (float)hemiStacks) + pi * 0.5f; // [PI/2..PI]
            float y = radius * cosf(phi) - cylinderHeight * 0.5f;               // shifted down by -H/2
            float r_xz = radius * sinf(phi);

            for (int j = 0; j <= slices; ++j)
            {
                float theta = (float)j * (twoPi / (float)slices);
                float x = r_xz * cosf(theta);
                float z = r_xz * sinf(theta);

                XMFLOAT3 pos(x, y, z);
                XMVECTOR p = XMLoadFloat3(&pos);
                XMVECTOR n = XMVector3Normalize(XMVectorSet(x, y + cylinderHeight * 0.5f, z, 0.0f)); // normal from local sphere center
                XMFLOAT3 normal{}; XMStoreFloat3(&normal, n);

                float u = theta / twoPi;
                float v = phi / pi; // map bottom hemi v [0.5..1]

                vertices.push_back(Vertex{ pos, normal, XMFLOAT2(u, v) });
            }
        }

        // Build indices across the three segments: top hemi, cylinder strip(s), bottom hemi
        auto idx = [slices](int row, int col) { return static_cast<uint32_t>(row * (slices + 1) + col); };

        int rowsTopHemi = (hemiStacks + 1);
        int rowsCylinder = cylStacks;
        int rowsBottomHemi = (hemiStacks + 1);

        // Total rows in vertex grid progression
        // Created vertices in order: rowsTopHemi, then rowsCylinder, then rowsBottomHemi
        int totalRows = rowsTopHemi + rowsCylinder + rowsBottomHemi;

        // Generate strip triangles for each adjacent row pair
        // CW winding consistent with reference
        int currentRowStart = 0;
        for (int row = 0; row < totalRows - 1; ++row)
        {
            // Each row has (slices + 1) verts
            int rowA = row;
            int rowB = row + 1;

            for (int j = 0; j < slices; ++j)
            {
                uint32_t a  = idx(rowA, j);
                uint32_t b  = idx(rowB, j);
                uint32_t a1 = idx(rowA, j + 1);
                uint32_t b1 = idx(rowB, j + 1);

                // CW
                indices.push_back(a);  indices.push_back(a1); indices.push_back(b);
                indices.push_back(a1); indices.push_back(b1); indices.push_back(b);
            }
        }

        return CreateMeshBuffers(device, vertices, indices);
    }
}