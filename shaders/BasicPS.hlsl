Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

// Texture sampling is the process where the GPU looks up a color from a texture (an image) using UV coordinates,
// and returns that color to your shader

// Flow of pixel shader: input from vertex shader -> sample texture -> compute lighting -> output final color

// Lighting implementation based on PBR (Physically Based Rendering) using Cook-Torrance model
// This model simulates realistic light interaction with surfaces using microfacet theory
// Flow of lighting computation: Fresnel -> Distribution -> Geometry -> Final color

// final pixel color is computed as:
// Pixel Color = Ambient + ((Diffuse + Specular) * Light Radiance * Angle of Incidence)
// where:
// - Ambient: small constant term to avoid pure black in unlit areas; typically a fraction of albedo
// - Diffuse: light scattered equally in all directions; depends on albedo and angle between light and normal
// - Specular: light reflected in a specific direction based on view and light angles; depends on Fresnel, Distribution, and Geometry terms
// - Light Radiance: color and intensity of the light source; determines how much light is emitted
// - Angle of Incidence: cosine of angle between light direction and surface normal; affects brightness based on orientation


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
// this function approximates how much light is reflected vs refracted at different angles
// F0 is the base reflectivity at normal incidence
// cosTheta is the angle between view direction and half-vector
// returns the reflectance color
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // flow:  1. compute (1 - cosTheta)
    //        2. raise to the 5th power
    //        3. scale by (1 - F0)
    //        4. add F0
    // This gives a smooth interpolation from F0 at normal incidence to nearly 1 at grazing angles
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}


// GGX/Trowbridge-Reitz normal distribution function
// this function describes the distribution of microfacets on a surface
// rougher surfaces have wider distributions, smoother surfaces have tighter distributions
// N is the surface normal, H is the half-vector between view and light directions
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;        // remap roughness to alpha
    float a2 = a * a;                       // alpha squared
    float NdotH = saturate(dot(N, H));      // cosine of angle between normal and half-vector
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);  // GGX denominator
    denom = PI * denom * denom;             
    return num / max(denom, 1e-7);              // prevent divide by zero
}


// Geometry term (Schlick-GGX)
// this function approximates the shadowing and masking of microfacets
// NdotX is the cosine of angle between normal and view/light direction
float GeometrySchlickGGX(float NdotX, float roughness)
{
    //float r = roughness;
    float k = (roughness + 1.0);
    k = (k * k) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}


// Smith's method for combined geometry term
// combines the geometry terms for both view and light directions
// N is the surface normal, V is the view direction, L is the light direction
// results in more realistic shadowing/masking effects
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));                      // cosine of angle between normal and view direction
    float NdotL = saturate(dot(N, L));                      // cosine of angle between normal and light direction
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);      // geometry term for view direction
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);      // geometry term for light direction
    return ggx1 * ggx2;                                     // combined geometry term
}


float4 main(PSInput input) : SV_Target
{
    // Sample base color (albedo)
    float4 albedoTex = g_Texture.Sample(g_Sampler, input.texCoord);
    float3 albedo = albedoTex.rgb;

    // Setup vectors
    float3 N = normalize(input.normal);                     // normal
    float3 V = normalize(g_CameraPos - input.worldPos);     // view direction
    float3 L = normalize(-g_LightDir);                      // light direction (from surface to light), negate: want surface->light
    float3 H = normalize(V + L);                            // half-vector (used to calculate how glossy the surface is)

    // Light radiance
    float3 radiance = g_LightColor * g_LightIntensity;

    // Base reflectivity F0: ~0.04 for dielectrics, albedo for metals
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, saturate(g_Metallic));

    // Cook-Torrance BRDF terms
    // The Cook-Torrance model combines microfacet distribution, geometry, and Fresnel terms to simulate realistic specular reflection
    // D: normal distribution, G: geometry, F: fresnel
    float D = DistributionGGX(N, H, g_Roughness);
    float G = GeometrySmith(N, V, L, g_Roughness);
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float3 F = FresnelSchlick(saturate(dot(H, V)), F0);

    float3 numerator = D * G * F;
    float denom = max(4.0 * NdotV * NdotL, 1e-4);
    float3 specular = numerator / denom;

    // Energy conservation; ensures that the surface does not reflect more light than it receives
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