// UnlitVS.hlsl
// Transforms local position using existing World/View/Projection constant buffers.

cbuffer CB_Projection : register(b0)
{
    row_major float4x4 proj;
};

cbuffer CB_View : register(b1)
{
    row_major float4x4 view;
};

cbuffer CB_World : register(b2)
{
    row_major float4x4 world;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
};

VSOutput main(VSInput v)
{
    VSOutput o;
    float4 wp = mul(float4(v.pos, 1.0f), world);
    float4 vp = mul(wp, view);
    o.pos = mul(vp, proj);
    return o;
}