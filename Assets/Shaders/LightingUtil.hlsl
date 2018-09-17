#define PI 3.14159265359
#define SHADOW_MAP_RESOLUTION 2048.0f

float3 DiffuseBurley(float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH)
{
	float roughnessSq = Roughness * Roughness;
	float FD90 = 0.5f + (2 * VoH * VoH * roughnessSq);
	float FdV = 1.0f + ((FD90 - 1.0f) * pow((1.0f - NoV), 5.0f));
	float FdL = 1.0f + ((FD90 - 1.0f) * pow((1.0f - NoL), 5.0f));
	return DiffuseColor * ((1 / PI) * FdV * FdL);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return (F0 + ((1.0f - F0) * pow((1.0f - cosTheta), 5.0f)));
}

float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r*r) / 8.0f;

	float num = NdotV;
	float denom = NdotV * (1.0f - k) + k;

	return num / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
	return ggx1 * ggx2;
}

float CalculateShadow(float4 shadowPosH, Texture2D ShadowMap, SamplerState gsamShadow)
{
	float currentDepth = shadowPosH.z;

    // Texel size.
    float dx = 1.0f / SHADOW_MAP_RESOLUTION;
	float dy = 1.0f / SHADOW_MAP_RESOLUTION;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dy), float2(0.0f,  -dy), float2(dx,  -dy),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dy), float2(0.0f,  +dy), float2(dx,  +dy)
    };

	float sampleDepth = 0.0f;
	
    [unroll]
    for(int i = 0; i < 9; ++i)
    {
		sampleDepth = ShadowMap.Sample(gsamShadow, shadowPosH.xy + offsets[i]).a;
		percentLit += ((currentDepth - 0.005f) > sampleDepth ? 1.0f : 0.0f);
    }
    
    float shadow = percentLit / 9.0f;
	
	return shadow;
}