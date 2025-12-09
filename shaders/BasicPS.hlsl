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
struct LightData
{
    float3 position;    // For Point/Spot
    float range;        // For Point/Spot attenuation
    float3 direction;   // For Directional/Spot
    float spotAngle;    // For Spot cone
    float3 color;
    float intensity;
    uint type;          // 0=Dir, 1=Point, 2=Spot
    float3 padding;     // pad to 16B alignment
};


// the number of lights is hardcoded for now since HLSL needs to know array sizes at compile time
cbuffer CB_Light : register(b3)
{
    float3 g_CameraPos;
    uint g_LightCount;
    LightData g_Lights[4]; // MUST MATCH MAX_LIGHTS
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


// Attenuation approximation for point/spot lights.
// For PBR, an inverse-square law is preferred; here a soft range cutoff with inverse-square is combined.
float ComputeAttenuation(float distance, float range)
{
    float soft = pow(saturate(1.0 - pow(distance / max(range, 1e-4), 4.0)), 2.0);
    float invSq = 1.0 / (distance * distance + 1.0);
    return soft * invSq;
}


float4 main(PSInput input) : SV_Target
{
    // Sample base color (albedo)
    float4 albedoTex = g_Texture.Sample(g_Sampler, input.texCoord);
    float3 albedo = albedoTex.rgb;

    // Setup base vectors
    float3 N = normalize(input.normal);                     // normal
    float3 V = normalize(g_CameraPos - input.worldPos);     // view direction

    // Base reflectivity F0: ~0.04 for dielectrics, albedo for metals
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, saturate(g_Metallic));

    // Accumulate lighting from all active lights
    float3 Lo = float3(0.0, 0.0, 0.0);

    [loop]
    for (uint i = 0; i < g_LightCount; ++i)
    {
        LightData light = g_Lights[i];

        // Compute L and attenuation depending on light type
        float3 L = float3(0, 0, 0);
        float attenuation = 1.0;

        if (light.type == 0) // Directional
        {
            L = normalize(-light.direction);
            attenuation = 1.0;
        }
        else
        {
            float3 toLight = light.position - input.worldPos;
            float dist = length(toLight);
            L = (dist > 1e-4) ? (toLight / dist) : float3(0, 0, 0);
            attenuation = ComputeAttenuation(dist, light.range);

            if (light.type == 2) // Spot
            {
                // test cone: compare angle between spotlight direction and L
                float theta = dot(normalize(-light.direction), L);
                // inside cone if angle less than spotAngle (use cos for comparison)
                if (theta <= cos(light.spotAngle))
                {
                    attenuation = 0.0;
                }
                // edge smoothening can be added here later
            }
        }

        // Radiance per light
        float3 radiance = light.color * light.intensity * attenuation;

        // Cook-Torrance BRDF terms per-light
        float3 H = normalize(V + L);
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

        float3 diffuse = (kD * albedo) / PI;

        // Accumulate contribution of this light
        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // Add a small ambient term to avoid pure black in unlit areas
    float3 ambient = 0.03 * albedo;
    float3 color = ambient + Lo;

    return float4(color, albedoTex.a);
}