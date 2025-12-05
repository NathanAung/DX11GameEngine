#include "Engine/MeshManager.h"
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Engine
{
    int MeshManager::InitializeCube(ID3D11Device* device)
    {
        const float s = 0.5f;
        std::vector<Vertex> vertices =
        {
            {{-s,-s,-s},{0,0,-1},{0,0}}, {{-s,+s,-s},{0,0,-1},{0,0}}, {{+s,+s,-s},{0,0,-1},{0,0}}, {{+s,-s,-s},{0,0,-1},{0,0}}, // back
            {{-s,-s,+s},{0,0,-1},{0,0}}, {{-s,+s,+s},{0,0,-1},{0,0}}, {{+s,+s,+s},{0,0,-1},{0,0}}, {{+s,-s,+s},{0,0,-1},{0,0}}  // front
        };
        std::vector<uint16_t> indices =
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
        ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = indices.data();

        ComPtr<ID3D11Buffer> ib;
        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, ib.GetAddressOf())))
            return -1;

        MeshData md{};
        md.vb = vb;
        md.ib = ib;
        md.indexCount = static_cast<UINT>(indices.size());
        md.stride = sizeof(Vertex);
        md.idxFmt = DXGI_FORMAT_R16_UINT;

		m_meshes.emplace(101, std::move(md));   // store as mesh ID 101 (temporary)
		return 101; // temporary mesh ID
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