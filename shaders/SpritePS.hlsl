Texture2D spriteTexture : register(t0);
SamplerState g_Sampler : register(s0);

// Tint and Alpha control
cbuffer CB_Color : register(b0)
{
    float4 tintColor;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    // Sample the texture
    float4 texColor = spriteTexture.Sample(g_Sampler, input.texCoord);
    
    // Multiply by the global tint (modifies both RGB and Alpha)
    return texColor * tintColor;
}