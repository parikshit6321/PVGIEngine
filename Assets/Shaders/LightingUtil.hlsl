#define PI 3.14159265359f
#define SHADOW_MAP_RESOLUTION 2048.0f
#define SHADOW_MAP_RESOLUTION_INV 0.00048828125f

float3 FresnelSchlick (float3 f0, float f90, float cosTheta)
{
	return f0 + (f90 - f0) * pow ((1.0f - cosTheta), 5.0f);
}

float SmithGGXCorrelated (float NdotL, float NdotV, float alphaG)
{
	float alphaG2 = alphaG * alphaG;
	// Caution : the " NdotL *" and " NdotV *" are explicitely inversed , this is not a mistake .
	float Lambda_GGXV = NdotL * sqrt((- NdotV * alphaG2 + NdotV) * NdotV + alphaG2);
	float Lambda_GGXL = NdotV * sqrt((- NdotL * alphaG2 + NdotL) * NdotL + alphaG2);

	return (0.5f / (Lambda_GGXV + Lambda_GGXL));
}

float D_GGX(float NdotH, float m)
{
	// Divide by PI is apply later
	float m2 = m * m;
	float f = (NdotH * m2 - NdotH) * NdotH + 1.0f;
	return (m2 / (f * f));
}

float3 DiffuseDisneyNormalized(float3 DiffuseColor, float Roughness, float NdotV, float NdotL, float LdotH)
{
	float energyBias = lerp(0.0f, 0.5f, Roughness);
	float energyFactor = lerp(1.0f, 1.0f / 1.51f, Roughness);
	
	float fd90 = energyBias + 2.0f * LdotH * LdotH * Roughness ;
	float3 f0 = float3(1.0f, 1.0f, 1.0f);
	
	float lightScatter = FresnelSchlick (f0, fd90, NdotL).r;
	float viewScatter = FresnelSchlick (f0, fd90, NdotV).r;
	
	return (DiffuseColor * lightScatter * viewScatter * energyFactor);
}

float CalculateShadow(float4 shadowPosH, Texture2D ShadowMap, SamplerState gsamShadow)
{
	float percentLit = 0.0f;
	
    const float2 offsets[9] =
    {
        float2(-SHADOW_MAP_RESOLUTION_INV,  -SHADOW_MAP_RESOLUTION_INV), float2(0.0f,  -SHADOW_MAP_RESOLUTION_INV), float2(SHADOW_MAP_RESOLUTION_INV,  -SHADOW_MAP_RESOLUTION_INV),
        float2(-SHADOW_MAP_RESOLUTION_INV, 0.0f), float2(0.0f, 0.0f), float2(SHADOW_MAP_RESOLUTION_INV, 0.0f),
        float2(-SHADOW_MAP_RESOLUTION_INV,  SHADOW_MAP_RESOLUTION_INV), float2(0.0f,  SHADOW_MAP_RESOLUTION_INV), float2(SHADOW_MAP_RESOLUTION_INV,  SHADOW_MAP_RESOLUTION_INV)
    };

	float sampleDepth = 0.0f;
	
    [unroll]
    for(int i = 0; i < 9; ++i)
    {
		sampleDepth = ShadowMap.Sample(gsamShadow, shadowPosH.xy + offsets[i]).a;
		percentLit += ((shadowPosH.z - 0.005f) > sampleDepth ? 1.0f : 0.0f);
    }
    
    percentLit *= 0.1111111111f;
	
	return percentLit;
}