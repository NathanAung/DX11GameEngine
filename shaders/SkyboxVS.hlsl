struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

// CBs (match existing registers)
// b0=Projection, b1=View, b2=World (same layout as BasicVS)
cbuffer CB_Projection : register(b0)
{
    row_major float4x4 g_Projection;
}

cbuffer CB_View : register(b1)
{
    row_major float4x4 g_View;
}

cbuffer CB_World : register(b2)
{
    row_major float4x4 g_World;
}

VSOutput main(VSInput input)
{
    VSOutput o;

    // Use vertex position as sampling direction for cubemap
    o.texCoord = input.position;

    // Remove translation from View so the skybox follows camera
    float4x4 viewNoTranslation = g_View;
    viewNoTranslation._41 = 0.0f;
    viewNoTranslation._42 = 0.0f;
    viewNoTranslation._43 = 0.0f;

    float4 p = mul(float4(input.position, 1.0f), mul(g_World, mul(viewNoTranslation, g_Projection)));

    // Z-trick: set z = w to push it to far plane (depth=1)
    o.pos = float4(p.x, p.y, p.w, p.w);
    return o;
}