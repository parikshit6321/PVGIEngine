#include "ConeTracingUtil.hlsl"

[numthreads(4, 4, 4)]
void CS(uint3 id : SV_DispatchThreadID)
{
	// Compute the current cell's world space position
	
	// Find the position first in range [0..1]
	float3 cellWorldPosition = ((float)id.xyz / (worldBoundary_R_ConeStep_G_HalfCellWidth_B_voxelResolution_A.r / worldBoundary_R_ConeStep_G_HalfCellWidth_B_voxelResolution_A.b));
	
	// Convert [0..1] range to [-1..1] range
	cellWorldPosition *= 2.0f;
	cellWorldPosition -= float3(1.0f, 1.0f, 1.0f);
	
	// Convert [-1..1] range to [-worldVolumeBoundary..worldVolumeBoundary] range
	cellWorldPosition *= worldBoundary_R_ConeStep_G_HalfCellWidth_B_voxelResolution_A.r;
	
	float4 accumulatedSHRed = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 accumulatedSHGreen = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 accumulatedSHBlue = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 worldNormal = float3(0.0f, 1.0f, 0.0f);
	
	float3 direction1 = normalize(cross(worldNormal, RANDOM_VECTOR));
	float3 direction3 = normalize(cross(worldNormal, direction1));	// Not used in cone tracing
	
	float direction1LerpValue = (direction1 * 0.3333f);
	float direction3LerpValue = (direction3 * 0.6667f);
	
	float3 direction5 = normalize(direction3LerpValue + direction1LerpValue);
	float3 direction7 = normalize(direction3LerpValue - direction1LerpValue);
	
	float3 worldNormalLerpValue = (worldNormal * 0.3333f);
	direction1LerpValue = (direction1 * 0.6667f);
	float3 direction5LerpValue = (direction5 * 0.6667f);
	float3 direction7LerpValue = (direction7 * 0.6667f);
	
	float3 coneDirections[7];
	coneDirections[0] = worldNormal;
	coneDirections[1] = normalize(worldNormalLerpValue + direction1LerpValue);
	coneDirections[2] = normalize(worldNormalLerpValue - direction1LerpValue);
	coneDirections[3] = normalize(worldNormalLerpValue + direction5LerpValue);
	coneDirections[4] = normalize(worldNormalLerpValue - direction5LerpValue);
	coneDirections[5] = normalize(worldNormalLerpValue + direction7LerpValue);
	coneDirections[6] = normalize(worldNormalLerpValue - direction7LerpValue);

	float3 coneTracedColor;
	float4 coeffs;
	
	for (uint iter1 = 0; iter1 < 7; ++iter1)
	{
		coneTracedColor = DiffuseConeTrace(cellWorldPosition, coneDirections[iter1]);
		coeffs = (dirToCosineLobe(coneDirections[iter1]));
		accumulatedSHRed += (coeffs * coneTracedColor.r);
		accumulatedSHGreen += (coeffs * coneTracedColor.g);
		accumulatedSHBlue += (coeffs * coneTracedColor.b);
	}

	for (uint iter2 = 0; iter2 < 7; ++iter2)
	{
		coneTracedColor = DiffuseConeTrace(cellWorldPosition, -coneDirections[iter2]);
		coeffs = (dirToCosineLobe(-coneDirections[iter2]));
		accumulatedSHRed += (coeffs * coneTracedColor.r);
		accumulatedSHGreen += (coeffs * coneTracedColor.g);
		accumulatedSHBlue += (coeffs * coneTracedColor.b);
	}
	
	shGridRed[id.xyz] = accumulatedSHRed * PI_INVERSE;
	shGridGreen[id.xyz] = accumulatedSHGreen * PI_INVERSE;
	shGridBlue[id.xyz] = accumulatedSHBlue * PI_INVERSE;
}