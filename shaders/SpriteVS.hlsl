cbuffer CB_Application : register(b0) // Orthographic Screen Matrix
{
    row_major float4x4 g_Projection;
};

cbuffer CB_Frame : register(b1) // Hijack the unused View matrix to pass UV rects
{
    row_major float4x4 g_View;
};

cbuffer CB_Object : register(b2) // Pixel Transform (X, Y, Width, Height)
{
    row_major float4x4 g_World;
};

// Must match Engine::Vertex layout
struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    
    // Scale and translate the 1x1 quad using the World matrix
    float4 worldPos = mul(float4(input.position, 1.0f), g_World);
    
    // Project directly to the screen (no camera View matrix needed)
    o.position = mul(worldPos, g_Projection);
    
    // Extract UV data from the first row of g_View: [uOffset, vOffset, uScale, vScale]
    float4 uvData = g_View[0];
    o.texCoord = input.texCoord * uvData.zw + uvData.xy;
    
    return o;
}