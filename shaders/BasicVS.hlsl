cbuffer CB_Application : register(b0) // Projection
{
    row_major float4x4 g_Projection;
};
cbuffer CB_Frame : register(b1) // View
{
    row_major float4x4 g_View;
};
cbuffer CB_Object : register(b2) // World
{
    row_major float4x4 g_World;
};


struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};


struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 worldPos : POSITION; // pass world space pos
};


VSOutput main(VSInput input)
{
    VSOutput o;
    float4 pos = float4(input.position, 1.0f);

    // Row-major path: vector is row, matrices are row-major. Use mul(row, M).
    float4 worldPos4 = mul(pos, g_World);
    float4 viewPos = mul(worldPos4, g_View);
    o.position = mul(viewPos, g_Projection);

    // Transform normal by World's upper-left 3x3 (rotation/scale) and normalize
    // Pass through texCoord (no tangent basis transform yet)
    o.normal = normalize(mul(input.normal, (float3x3) g_World));
    o.texCoord = input.texCoord;

    // world position for view direction
    o.worldPos = worldPos4.xyz;
    return o;
}