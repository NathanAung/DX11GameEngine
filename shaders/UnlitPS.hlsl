// UnlitPS.hlsl
// Outputs a solid color using a dedicated color constant buffer.

// register(b0) is used to avoid conflicts with the World/View/Projection constant buffers in UnlitVS.hlsl.
// b0 is also used by per-object constants in BasicPS.hlsl, but since UnlitPS doesn't use those, it's safe to reuse b0 for the solid color.
cbuffer ColorConstants : register(b0)
{
    float4 solidColor;
};

float4 main() : SV_TARGET
{
    return solidColor;
}