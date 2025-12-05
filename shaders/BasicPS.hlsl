Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    // Sample texture using UVs, fallback to white if no texture is bound is handled by runtime binding
    return g_Texture.Sample(g_Sampler, input.texCoord);
}