struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    // Solid white for now
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}