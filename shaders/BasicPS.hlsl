Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

// Texture sampling is the process where the GPU looks up a color from a texture (an image) using UV coordinates,
// and returns that color to your shader

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 worldPos : POSITION; // match VSOutput
};


// Must match C++ LightConstants layout exactly (register b3)
cbuffer CB_Light : register(b3)
{
    float3 g_LightDir;
    float pad1; // direction (from light toward world). negate for surface-to-light.
    float3 g_LightColor;
    float g_LightIntensity;
    float3 g_CameraPos;
    float pad2;
}


// Simple material constants (register b4)
cbuffer CB_Material : register(b4)
{
    float g_Roughness;
    float g_Metallic;
    float2 g_Padding; // pad to 16B
}


static const float PI = 3.14159265359;


// Fresnel Schlick approximation
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}


// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 1e-7);
}


// Geometry term (Schlick-GGX)
float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness;
    float k = (r + 1.0);
    k = (k * k) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}


// Smith's method for combined geometry term
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}


float4 main(PSInput input) : SV_Target
{
    // Sample base color (albedo)
    float4 albedoTex = g_Texture.Sample(g_Sampler, input.texCoord);
    float3 albedo = albedoTex.rgb;

    // Setup vectors
    float3 N = normalize(input.normal);
    float3 V = normalize(g_CameraPos - input.worldPos);
    float3 L = normalize(-g_LightDir); // negate: want surface->light
    float3 H = normalize(V + L);

    // Light radiance
    float3 radiance = g_LightColor * g_LightIntensity;

    // Base reflectivity F0: ~0.04 for dielectrics, albedo for metals
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, saturate(g_Metallic));

    // Cook-Torrance BRDF terms
    float D = DistributionGGX(N, H, g_Roughness);
    float G = GeometrySmith(N, V, L, g_Roughness);
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float3 F = FresnelSchlick(saturate(dot(H, V)), F0);

    float3 numerator = D * G * F;
    float denom = max(4.0 * NdotV * NdotL, 1e-4);
    float3 specular = numerator / denom;

    // Energy conservation
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= (1.0 - saturate(g_Metallic)); // metals have no diffuse

    // Final lighting (one directional light)
    float3 diffuse = (kD * albedo) / PI;
    float3 Lo = (diffuse + specular) * radiance * NdotL;

    // Add a small ambient term to avoid pure black in unlit areas
    float3 ambient = 0.03 * albedo;
    float3 color = ambient + Lo;

    return float4(color, albedoTex.a);
}