cbuffer CB_Application : register(b0) // Light View-Projection Matrix
{
    row_major float4x4 g_LightViewProj;
};

cbuffer CB_Object : register(b2) // Entity World Matrix
{
    row_major float4x4 g_World;
};

struct VSInput
{
    float3 position : POSITION;
    // normal, texcoord, and tangent are ignored for the shadow pass
};

float4 main(VSInput input) : SV_POSITION
{
    float4 worldPos = mul(float4(input.position, 1.0f), g_World);
    return mul(worldPos, g_LightViewProj);
}