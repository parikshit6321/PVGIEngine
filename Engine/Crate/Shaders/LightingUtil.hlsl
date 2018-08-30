#define PI 3.14159265359

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = ((2.0f * normalMapSample) - 1.0f);

	// Build orthonormal basis.
	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

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