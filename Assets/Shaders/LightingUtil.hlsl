#define PI 3.14159265359f
#define SHADOW_MAP_RESOLUTION 2048.0f
#define SHADOW_MAP_RESOLUTION_INV 0.00048828125f

inline float3 FresnelSchlick (float3 f0, float f90, float cosTheta)
{
	return f0 + (f90 - f0) * pow ((1.0f - cosTheta), 5.0f);
}

inline float SmithGGXCorrelated (float NdotL, float NdotV, float alphaG2)
{
	float Lambda_GGXV = NdotL * sqrt((- NdotV * alphaG2 + NdotV) * NdotV + alphaG2);
	float Lambda_GGXL = NdotV * sqrt((- NdotL * alphaG2 + NdotL) * NdotL + alphaG2);

	return (0.5f / (Lambda_GGXV + Lambda_GGXL));
}

inline float D_GGX(float NdotH, float m2)
{
	float f = (NdotH * m2 - NdotH) * NdotH + 1.0f;
	return (m2 / (f * f));
}

inline float3 LambertianDiffuse(float3 DiffuseColor)
{
	return (DiffuseColor / PI);
}

inline float CalculateShadow(float4 shadowPosH, Texture2D ShadowMap, SamplerComparisonState gsamShadow)
{
	float percentLit = 0.0f;
	
    const float2 offsets[9] =
    {
        float2(-SHADOW_MAP_RESOLUTION_INV,  -SHADOW_MAP_RESOLUTION_INV), float2(0.0f,  -SHADOW_MAP_RESOLUTION_INV), float2(SHADOW_MAP_RESOLUTION_INV,  -SHADOW_MAP_RESOLUTION_INV),
        float2(-SHADOW_MAP_RESOLUTION_INV, 0.0f), float2(0.0f, 0.0f), float2(SHADOW_MAP_RESOLUTION_INV, 0.0f),
        float2(-SHADOW_MAP_RESOLUTION_INV,  SHADOW_MAP_RESOLUTION_INV), float2(0.0f,  SHADOW_MAP_RESOLUTION_INV), float2(SHADOW_MAP_RESOLUTION_INV,  SHADOW_MAP_RESOLUTION_INV)
    };

    [unroll]
    for(int i = 0; i < 9; ++i)
    {
		percentLit += ShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], shadowPosH.z).r;
	}
    
    percentLit *= 0.1111111111f;
	
	return percentLit;
}