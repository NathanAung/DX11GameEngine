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
    float3 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    float4 pos = float4(input.position, 1.0f);

    // Row-major path: vector is row, matrices are row-major. Use mul(row, M).
    float4 worldPos = mul(pos, g_World);
    float4 viewPos = mul(worldPos, g_View);
    o.position = mul(viewPos, g_Projection);

    o.color = input.color;
    return o;
}