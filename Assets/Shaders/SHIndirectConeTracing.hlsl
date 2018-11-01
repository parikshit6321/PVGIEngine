#include "ConeTracingUtil.hlsl"

[numthreads(2, 2, 2)]
void CS(uint3 id : SV_DispatchThreadID)
{
	// Compute the current cell's world space position
	
	// Find the position first in range [0..1]
	float3 cellWorldPosition = ((float)id.xyz / (float)voxelResolution);
	
	// Convert [0..1] range to [-1..1] range
	cellWorldPosition *= 2.0f;
	cellWorldPosition -= float3(1.0f, 1.0f, 1.0f);
	
	// Convert [-1..1] range to [-worldVolumeBoundary..worldVolumeBoundary] range
	cellWorldPosition *= worldVolumeBoundary;
	
	float4 accumulatedSHRed = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 accumulatedSHGreen = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 accumulatedSHBlue = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 worldNormal = float3(0.0f, 1.0f, 0.0f);
	
	float3 direction1 = normalize(cross(worldNormal, RANDOM_VECTOR));
	float3 direction2 = -direction1;
	float3 direction3 = normalize(cross(worldNormal, direction1));	// Not used in cone tracing
	float3 direction4 = -direction3; 								// Not used in cone tracing
	float3 direction5 = lerp(direction1, direction3, 0.6667f);
	float3 direction6 = -direction5;
	float3 direction7 = lerp(direction2, direction3, 0.6667f);
	float3 direction8 = -direction7;
	
	float3 coneDirections[7];
	coneDirections[0] = worldNormal;
	coneDirections[1] = normalize(lerp(direction1, worldNormal, 0.3333f));
	coneDirections[2] = normalize(lerp(direction2, worldNormal, 0.3333f));
	coneDirections[3] = normalize(lerp(direction5, worldNormal, 0.3333f));
	coneDirections[4] = normalize(lerp(direction6, worldNormal, 0.3333f));
	coneDirections[5] = normalize(lerp(direction7, worldNormal, 0.3333f));
	coneDirections[6] = normalize(lerp(direction8, worldNormal, 0.3333f));

	float3 coneTracedColor = float3(0.0f, 0.0f, 0.0f);
	float4 coeffs = float4(0.0f, 0.0f, 0.0f, 0.0f);
	
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
	
	shGridRed[id.xyz] = accumulatedSHRed / PI;
	shGridGreen[id.xyz] = accumulatedSHGreen / PI;
	shGridBlue[id.xyz] = accumulatedSHBlue / PI;
}