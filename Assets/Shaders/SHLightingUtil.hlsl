#include "LightingUtil.hlsl"

// Spherical harmonics coefficients – precomputed
#define SH_C0 0.282094792f // 1 / 2sqrt(pi)
#define SH_C1 0.488602512f // sqrt(3/pi) / 2

#define PI_INVERSE 0.31830988618f

Texture3D	 SHGridRed				 : register(t5);
Texture3D	 SHGridGreen			 : register(t6);
Texture3D	 SHGridBlue				 : register(t7);

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
	float4 worldBoundary_R_ConeStep_G_HalfCellWidth_B;
};

inline float4 dirToSH(float3 dir) {
	return float4(SH_C0, -SH_C1 * dir.y, SH_C1 * dir.z, -SH_C1 * dir.x);
}

// Function to get position of cell in the SH grid from world position
inline float3 GetCellPosition (float3 worldPosition)
{
	float3 encodedPosition = worldPosition / worldBoundary_R_ConeStep_G_HalfCellWidth_B.r;
	encodedPosition += float3(1.0f, 1.0f, 1.0f);
	encodedPosition *= 0.5f;
	return encodedPosition;
}

inline float3 SampleSHIndirectLighting(float3 worldPosition, float3 worldNormal)
{
	worldPosition += (worldNormal * worldBoundary_R_ConeStep_G_HalfCellWidth_B.b);

	float3 cellPosition = GetCellPosition(worldPosition);

	float4 currentCellRedSH = SHGridRed.Sample(gsamLinearWrap, cellPosition);
	float4 currentCellGreenSH = SHGridGreen.Sample(gsamLinearWrap, cellPosition);
	float4 currentCellBlueSH = SHGridBlue.Sample(gsamLinearWrap, cellPosition);

	float4 SHintensity = dirToSH(-worldNormal);
	
	float3 indirect = float3(saturate(dot(SHintensity, currentCellRedSH)), saturate(dot(SHintensity, currentCellGreenSH)), saturate(dot(SHintensity, currentCellBlueSH)));
	
	return indirect;
}