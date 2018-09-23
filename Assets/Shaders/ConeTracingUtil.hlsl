#include "LightingUtil.hlsl"

// For maximum 64 iterations
#define ITERATIONS_0 2.0f
#define ITERATIONS_1 2.0f
#define ITERATIONS_2 4.0f
#define ITERATIONS_3 8.0f
#define ITERATIONS_4 16.0f
#define ITERATIONS_5 32.0f

#define RANDOM_VECTOR float3(0.267261f, 0.534522f, 0.801784f)

Texture3D	 VoxelGrid1				 : register(t5);
Texture3D	 VoxelGrid2				 : register(t6);
Texture3D	 VoxelGrid3				 : register(t7);
Texture3D	 VoxelGrid4				 : register(t8);
Texture3D	 VoxelGrid5				 : register(t9);

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

inline float3 ConeTrace(float3 worldPosition, float3 coneDirection, float3 worldNormal)
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

inline float3 CalculateIndirectLighting(float3 worldPosition, float3 worldNormal, float3 V, float linearRoughness, float3 diffuseColor)
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

	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection1)), 
												saturate(dot(-coneDirection1, normalize(-coneDirection1 + V)))) * 
												ConeTrace(worldPosition, coneDirection1, worldNormal));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection2)), 
												saturate(dot(-coneDirection2, normalize(-coneDirection2 + V)))) * 
												ConeTrace(worldPosition, coneDirection2, worldNormal));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection3)), 
												saturate(dot(-coneDirection3, normalize(-coneDirection3 + V)))) * 
												ConeTrace(worldPosition, coneDirection3, worldNormal));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection4)), 
												saturate(dot(-coneDirection4, normalize(-coneDirection4 + V)))) * 
												ConeTrace(worldPosition, coneDirection4, worldNormal));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection5)), 
												saturate(dot(-coneDirection5, normalize(-coneDirection5 + V)))) * 
												ConeTrace(worldPosition, coneDirection5, worldNormal));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection6)), 
												saturate(dot(-coneDirection6, normalize(-coneDirection6 + V)))) * 
												ConeTrace(worldPosition, coneDirection6, worldNormal));
	accumulatedColor += (DiffuseDisneyNormalized(diffuseColor, linearRoughness, NdotV, saturate(dot(worldNormal, -coneDirection7)), 
												saturate(dot(-coneDirection7, normalize(-coneDirection7 + V)))) * 
												ConeTrace(worldPosition, coneDirection7, worldNormal));
	
	accumulatedColor /= PI;
	
	return accumulatedColor;
}