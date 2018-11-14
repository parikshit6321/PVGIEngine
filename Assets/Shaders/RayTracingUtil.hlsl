#include "LightingUtil.hlsl"

#define MAXIMUM_ITERATIONS 64.0f

#define RAY_STEP 0.2f
#define RAY_OFFSET 0.2f

#define PI_INVERSE 0.31830988618f

#define RANDOM_VECTOR float3(0.267261f, 0.534522f, 0.801784f)

Texture3D	 VoxelGrid0				 : register(t5);
Texture3D	 VoxelGrid2				 : register(t6);

SamplerState gsamLinearWrap			 : register(s0);

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

// Returns the voxel information from grid 2
inline float4 GetVoxelInfo2(float3 voxelPosition)
{
	float4 info = VoxelGrid2.Sample(gsamLinearWrap, voxelPosition);
	return info;
}

inline float3 SpecularRayTrace(float3 worldPosition, float3 rayDirection, float LinearRoughness)
{
	float3 currentPosition = worldPosition + (rayDirection * RAY_OFFSET);
	float3 currentVoxelPosition = float3(0.0f, 0.0f, 0.0f);
	float4 currentVoxelInfo0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 currentVoxelInfo2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
	
	// Ray tracing loop
	for (float i = 0.0f; i < MAXIMUM_ITERATIONS; i += 1.0f)
	{
		currentPosition += (RAY_STEP * rayDirection);
		currentVoxelPosition = GetVoxelPosition(currentPosition);

		if (currentVoxelInfo0.a < 0.05f)
		{
			currentVoxelInfo0 = GetVoxelInfo0(currentVoxelPosition);
		}
		
		if (currentVoxelInfo2.a < 0.05f)
		{
			currentVoxelInfo2 = GetVoxelInfo2(currentVoxelPosition);
		}
	}
	
	float3 resultingColor = lerp(currentVoxelInfo0.rgb, currentVoxelInfo2.rgb, LinearRoughness);

	return resultingColor;
}

inline float3 CalculateSpecularIndirectLighting(float3 worldPosition, float3 N, float3 V, float linearRoughness, float metallic, float3 albedo)
{
	float3 reflectedDirection = normalize(-V - (2.0f * dot(-V, N) * N));
	
	float3 rayTracedColor = SpecularRayTrace(worldPosition, reflectedDirection, linearRoughness);
	
	float3 L = -reflectedDirection;
	float3 H = normalize(V + L);

	float NdotV = abs(dot(N, V)) + 1e-5f; // avoid artifact
	float LdotH = saturate(dot(L ,H));
	float NdotH = saturate(dot(N ,H));
	float NdotL = saturate(dot(N ,L));

	// BRDF : GGX Specular

	float energyBias = lerp(0.0f, 0.5f, linearRoughness);
	float fd90 = energyBias + 2.0f * LdotH * LdotH * linearRoughness ;
	float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	
	// Specular BRDF
	float3 F = FresnelSchlick(f0, fd90, LdotH);
	float Vis = SmithGGXCorrelated(NdotV, NdotL, linearRoughness);
	float D = D_GGX(NdotH, linearRoughness);
	float3 Fr = D * F * Vis * PI_INVERSE;
	
	return (Fr * rayTracedColor * NdotL);
}