Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D ormMap : register(t2);
SamplerState g_Sampler : register(s0);

// Texture sampling is the process where the GPU looks up a color from a texture (an image) using UV coordinates,
// and returns that color to the shader

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
// - Angle of Incidence (NdotL): cosine of angle between light direction and surface normal; affects brightness based on orientation


struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION; // match VSOutput
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 Tangent : TANGENT;
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
    float4 baseColor;
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
    // step 1: Sample base color (albedo)
    float4 texColor = albedoMap.Sample(g_Sampler, input.texCoord) * baseColor;
    float3 albedo = texColor.rgb;

    // Sample ORM map and scale by material constants
    float3 orm = ormMap.Sample(g_Sampler, input.texCoord).rgb;
    float ao = orm.r;
    float roughness = orm.g * g_Roughness;
    float metallic = orm.b * g_Metallic;

    // Tangent Space Normal Mapping (TBN)
    float3 N = normalize(input.normal);
    float3 T = normalize(input.Tangent);
    
    // Gram-Schmidt orthogonalization to ensure T is perpendicular to N
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    
    // Construct the TBN matrix to transform from tangent space to world space
    float3x3 TBN = float3x3(T, B, N);

    // Sample the normal map and convert from [0, 1] color space to [-1, 1] vector space
    float3 sampledNormal = normalMap.Sample(g_Sampler, input.texCoord).xyz;
    sampledNormal = sampledNormal * 2.0f - 1.0f;

    // Calculate final bumped normal and overwrite N
    N = normalize(mul(sampledNormal, TBN));

    // step 2: compute view vector
    float3 V = normalize(g_CameraPos - input.worldPos); // view (camera) direction

    // step 3: compute base reflectivity F0
    // Use the unpacked local 'metallic' variable instead of the global 'g_Metallic'
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, saturate(metallic));

    // step 4: accumulate lighting from all active lights
    float3 Lo = float3(0.0, 0.0, 0.0); // outgoing light

    [loop]
    for (uint i = 0; i < g_LightCount; ++i)
    {
        LightData light = g_Lights[i];

        // Compute direction towards light (L) and attenuation depending on light type
        float3 L = float3(0, 0, 0);
        float attenuation = 1.0;

        if (light.type == 0) // Directional
        {
            // negative because surface to light
            L = normalize(-light.direction);
            attenuation = 1.0;
        }
        // Point or Spot
        else
        {
            // vector from surface point to light position
            float3 toLight = light.position - input.worldPos;
            // distance to light
            float dist = length(toLight);
            
            // normalize to get light direction
            // if very close to light, set L to zero vector to avoid NaNs
            // 1e-4 is arbitrary small threshold
            L = (dist > 1e-4) ? (toLight / dist) : float3(0, 0, 0);
            
            // compute attenuation based on distance and range
            attenuation = ComputeAttenuation(dist, light.range);

            if (light.type == 2) // Spot
            {
                // test cone: compare angle between spotlight direction and L
                // dot product gives cos of angle between vectors
                float theta = dot(normalize(-light.direction), L);
                // inside cone if angle less than spotAngle (use cos for comparison)
                // if outside cone, set attenuation to zero, effectively turning off light contribution
                if (theta <= cos(light.spotAngle))
                {
                    attenuation = 0.0;
                }
            }
        }

        // Radiance per light
        float3 radiance = light.color * light.intensity * attenuation;

        // Cook-Torrance BRDF terms per-light
        
        // half-vector, calculated as the normalized sum of view and light directions
        float3 H = normalize(V + L);
        // Distribution term, describes microfacet orientation
        float D = DistributionGGX(N, H, roughness);
        // Geometry term, accounts for shadowing/masking
        float G = GeometrySmith(N, V, L, roughness);
        // cosine of angle between normal and view direction, for energy conservation
        float NdotV = saturate(dot(N, V));
        // cosine of angle between normal and light direction, for energy conservation
        float NdotL = saturate(dot(N, L));
        // Fresnel term, describes reflectance at different angles
        float3 F = FresnelSchlick(saturate(dot(H, V)), F0);

        // numerator of Cook-Torrance BRDF is the product of D, G, and F
        float3 numerator = D * G * F;
        // denominator is 4 * NdotV * NdotL, clamped to avoid division by zero
        float denom = max(4.0 * NdotV * NdotL, 1e-4);
        // final specular term is numerator divided by denominator
        float3 specular = numerator / denom;

        // Energy conservation; ensures that the surface does not reflect more light than it receives
        float3 kS = F; // specular reflection component
        float3 kD = 1.0 - kS; // diffuse reflection component
        kD *= (1.0 - saturate(metallic));

        // Lambertian diffuse term, scaled by albedo and PI 
        float3 diffuse = (kD * albedo) / PI;

        // Accumulate contribution of this light
        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // step 5: add a small ambient term, shadowed by ambient occlusion
    float3 ambient = 0.03 * albedo * ao; // Multiplied by ao
    float3 color = ambient + Lo;

    return float4(color, texColor.a);
}