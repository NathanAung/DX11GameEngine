struct PSInput
{
    float4 pos : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

TextureCube g_TexCube : register(t0);
SamplerState g_Sam : register(s0);

float4 main(PSInput input) : SV_Target
{
    return g_TexCube.Sample(g_Sam, normalize(input.texCoord));
}