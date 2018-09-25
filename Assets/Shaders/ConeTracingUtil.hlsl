#include "LightingUtil.hlsl"

// For maximum 64 iterations
#define ITERATIONS_0 2.0f
#define ITERATIONS_1 2.0f
#define ITERATIONS_2 4.0f
#define ITERATIONS_3 8.0f
#define ITERATIONS_4 16.0f
#define ITERATIONS_5 32.0f

#define MAXIMUM_ITERATIONS 64.0f

#define RAY_STEP 0.2f
#define RAY_OFFSET 0.2f

#define RANDOM_VECTOR float3(0.267261f, 0.534522f, 0.801784f)

Texture3D	 VoxelGrid0				 : register(t5);
Texture3D	 VoxelGrid1				 : register(t6);
Texture3D	 VoxelGrid2				 : register(t7);
Texture3D	 VoxelGrid3				 : register(t8);
Texture3D	 VoxelGrid4				 : register(t9);
Texture3D	 VoxelGrid5				 : register(t10);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);

// Constant data that varies per material.
cbuffer cbPass : register(b0)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float userLUTContribution;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gSunLightStrength;
	float4 gSunLightDirection;
	float4x4 gSkyBoxMatrix;
	float4x4 gShadowViewProj;
	float4x4 gShadowTransform;
	float4 gWorldBoundary_R_ConeStep_G;
};

// Returns the voxel position in the grids
inline float3 GetVoxelPosition(float3 worldPosition)
{
	float3 voxelPosition = worldPosition / gWorldBoundary_R_ConeStep_G.r;
	voxelPosition += float3(1.0f, 1.0f, 1.0f);
	voxelPosition *= 0.5f;
	return voxelPosition;
}

// Returns the voxel information from grid 0
inline float4 GetVoxelInfo0(float3 voxelPosition)
{
	float4 info = VoxelGrid0.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

// Returns the voxel information from grid 1
inline float4 GetVoxelInfo1(float3 voxelPosition)
{
	float4 info = VoxelGrid1.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

// Returns the voxel information from grid 2
inline float4 GetVoxelInfo2(float3 voxelPosition)
{
	float4 info = VoxelGrid2.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

// Returns the voxel information from grid 3
inline float4 GetVoxelInfo3(float3 voxelPosition)
{
	float4 info = VoxelGrid3.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

// Returns the voxel information from grid 4
inline float4 GetVoxelInfo4(float3 voxelPosition)
{
	float4 info = VoxelGrid4.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

// Returns the voxel information from grid 5
inline float4 GetVoxelInfo5(float3 voxelPosition)
{
	float4 info = VoxelGrid5.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

inline float3 DiffuseConeTrace(float3 worldPosition, float3 coneDirection)
{
	float3 coneOrigin = worldPosition + (coneDirection * gWorldBoundary_R_ConeStep_G.g * ITERATIONS_0);

	float3 currentPosition = coneOrigin;
	float4 currentVoxelInfo = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float hitFound = 0.0f;

	// Sample voxel grid 1
	for (float i1 = 0.0f; i1 < ITERATIONS_1; i1 += 1.0f)
	{
		currentPosition += (gWorldBoundary_R_ConeStep_G.g * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo1(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	// Sample voxel grid 2
	for (float i2 = 0.0f; i2 < ITERATIONS_2; i2 += 1.0f)
	{
		currentPosition += (gWorldBoundary_R_ConeStep_G.g * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo2(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	// Sample voxel grid 3
	for (float i3 = 0.0f; i3 < ITERATIONS_3; i3 += 1.0f)
	{
		currentPosition += (gWorldBoundary_R_ConeStep_G.g * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo3(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	// Sample voxel grid 4
	for (float i4 = 0.0f; i4 < ITERATIONS_4; i4 += 1.0f)
	{
		currentPosition += (gWorldBoundary_R_ConeStep_G.g * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo4(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	// Sample voxel grid 5
	for (float i5 = 0.0f; i5 < ITERATIONS_5; i5 += 1.0f)
	{
		currentPosition += (gWorldBoundary_R_ConeStep_G.g * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo5(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	return currentVoxelInfo.rgb;
}

inline float3 CalculateDiffuseIndirectLighting(float3 worldPosition, float3 worldNormal, float3 V, float linearRoughness, float metallic, float3 diffuseColor)
{
	float NdotV = abs(dot(worldNormal, V)) + 1e-5f; // avoid artifact
	
	float3 accumulatedColor = float3(0.0f, 0.0f, 0.0f);

	float3 direction1 = normalize(cross(worldNormal, RANDOM_VECTOR));
	float3 direction2 = -direction1;
	float3 direction3 = normalize(cross(worldNormal, direction1));	// Not used in cone tracing
	float3 direction4 = -direction3; 								// Not used in cone tracing
	float3 direction5 = lerp(direction1, direction3, 0.6667f);
	float3 direction6 = -direction5;
	float3 direction7 = lerp(direction2, direction3, 0.6667f);
	float3 direction8 = -direction7;

	float3 coneDirection1 = worldNormal;
	float3 coneDirection2 = normalize(lerp(direction1, worldNormal, 0.3333f));
	float3 coneDirection3 = normalize(lerp(direction2, worldNormal, 0.3333f));
	float3 coneDirection4 = normalize(lerp(direction5, worldNormal, 0.3333f));
	float3 coneDirection5 = normalize(lerp(direction6, worldNormal, 0.3333f));
	float3 coneDirection6 = normalize(lerp(direction7, worldNormal, 0.3333f));
	float3 coneDirection7 = normalize(lerp(direction8, worldNormal, 0.3333f));

	// BRDF : Disney Diffuse
	
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection1)), 
												saturate(dot(-coneDirection1, normalize(-coneDirection1 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection1));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection2)), 
												saturate(dot(-coneDirection2, normalize(-coneDirection2 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection2));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection3)), 
												saturate(dot(-coneDirection3, normalize(-coneDirection3 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection3));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection4)), 
												saturate(dot(-coneDirection4, normalize(-coneDirection4 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection4));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection5)), 
												saturate(dot(-coneDirection5, normalize(-coneDirection5 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection5));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection6)), 
												saturate(dot(-coneDirection6, normalize(-coneDirection6 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection6));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection7)), 
												saturate(dot(-coneDirection7, normalize(-coneDirection7 + V)))) * 
												DiffuseConeTrace(worldPosition, coneDirection7));
	
	accumulatedColor *= (1.0f - metallic);
	accumulatedColor /= PI;
	
	return accumulatedColor;
}

inline float3 SpecularRayTrace(float3 worldPosition, float3 rayDirection, float LinearRoughness)
{
	float3 currentPosition = worldPosition + (rayDirection * RAY_OFFSET);
	float3 currentVoxelPosition = float3(0.0f, 0.0f, 0.0f);
	float4 currentVoxelInfo0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 currentVoxelInfo2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
	
	float hitFound0 = 0.0f;
	float hitFound2 = 0.0f;
	
	// Ray tracing loop
	for (float i = 0.0f; i < MAXIMUM_ITERATIONS; i += 1.0f)
	{
		currentPosition += (RAY_STEP * rayDirection);
		currentVoxelPosition = GetVoxelPosition(currentPosition);

		if (hitFound0 < 0.05f)
		{
			currentVoxelInfo0 = GetVoxelInfo0(currentVoxelPosition);
			hitFound0 = currentVoxelInfo0.a;
		}
		
		if (hitFound2 < 0.05f)
		{
			currentVoxelInfo2 = GetVoxelInfo2(currentVoxelPosition);
			hitFound2 = currentVoxelInfo2.a;
		}
	}
	
	float3 resultingColor = lerp(currentVoxelInfo0.rgb, currentVoxelInfo2.rgb, LinearRoughness);

	return resultingColor;
}

inline float3 CalculateSpecularIndirectLighting(float3 worldPosition, float3 N, float3 V, float roughness, float metallic, float3 albedo)
{
	float3 reflectedDirection = normalize(-V - (2.0f * dot(-V, N) * N));
	
	float3 rayTracedColor = SpecularRayTrace(worldPosition, reflectedDirection, roughness * roughness);
	
	float3 L = -reflectedDirection;
	float3 H = normalize(V + L);

	float NdotV = abs(dot(N, V)) + 1e-5f; // avoid artifact
	float LdotH = saturate(dot(L ,H));
	float NdotH = saturate(dot(N ,H));
	float NdotL = saturate(dot(N ,L));

	// BRDF : GGX Specular

	float energyBias = lerp(0.0f, 0.5f, roughness);
	float fd90 = energyBias + 2.0f * LdotH * LdotH * roughness ;
	float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	
	// Specular BRDF
	float3 F = FresnelSchlick(f0, fd90, LdotH);
	float Vis = SmithGGXCorrelated(NdotV, NdotL, roughness);
	float D = D_GGX(NdotH, roughness);
	float3 Fr = D * F * Vis / PI;
	
	return (Fr * rayTracedColor * NdotL);
}