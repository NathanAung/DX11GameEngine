// UnlitPS.hlsl
// Outputs a solid color using a dedicated color constant buffer.

cbuffer ColorConstants : register(b0)
{
    float4 solidColor;
};

float4 main() : SV_TARGET
{
    return solidColor;
}