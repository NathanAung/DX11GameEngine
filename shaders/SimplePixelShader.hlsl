// REFERENCE SCRIPT, UNUSED 
// A simple pixel shader that outputs the input color from the vertex shader
struct PixelShaderInput
{
    float4 color : COLOR;
};

// main pixel shader function (entry point)
// outputs the color to the bound render target by returning a value with the SV_TARGET semantic
// SV_TARGET: indicates the output color of the pixel shader
float4 SimplePixelShader(PixelShaderInput IN) : SV_TARGET
{
    return IN.color;
}