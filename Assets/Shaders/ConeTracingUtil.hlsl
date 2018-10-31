cbuffer cbSettings : register(b0)
{
	uint voxelResolution;
	float worldVolumeBoundary;
	float coneStep;
};

Texture3D VoxelGrid0 			: register(t0);
Texture3D VoxelGrid1 			: register(t1);
Texture3D VoxelGrid2 			: register(t2);
Texture3D VoxelGrid3 			: register(t3);
Texture3D VoxelGrid4 			: register(t4);
Texture3D VoxelGrid5 			: register(t5);

RWTexture3D<float4> shGridRed	: register(u0);
RWTexture3D<float4> shGridGreen	: register(u1);
RWTexture3D<float4> shGridBlue	: register(u2);

SamplerState gsamLinearWrap	: register(s0);

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

/*Cosine lobe coeff*/
#define SH_cosLobe_C0 0.886226925f // sqrt(pi)/2
#define SH_cosLobe_C1 1.02332671f // sqrt(pi/3)

#define PI 3.1415926f

inline float4 dirToCosineLobe(float3 dir) {
	return float4(SH_cosLobe_C0, -SH_cosLobe_C1 * dir.y, SH_cosLobe_C1 * dir.z, -SH_cosLobe_C1 * dir.x);
}

// Returns the voxel position in the grids
inline float3 GetVoxelPosition(float3 worldPosition)
{
	float3 voxelPosition = worldPosition / worldVolumeBoundary;
	voxelPosition += float3(1.0f, 1.0f, 1.0f);
	voxelPosition *= 0.5f;
	return voxelPosition;
}

// Returns the voxel information from grid 0
inline float4 GetVoxelInfo0(float3 voxelPosition)
{
	float4 info = VoxelGrid0.SampleLevel(gsamLinearWrap, voxelPosition, 0);
	return info;
}

// Returns the voxel information from grid 1
inline float4 GetVoxelInfo1(float3 voxelPosition)
{
	float4 info = VoxelGrid1.SampleLevel(gsamLinearWrap, voxelPosition, 0);
	return info;
}

// Returns the voxel information from grid 2
inline float4 GetVoxelInfo2(float3 voxelPosition)
{
	float4 info = VoxelGrid2.SampleLevel(gsamLinearWrap, voxelPosition, 0);
	return info;
}

// Returns the voxel information from grid 3
inline float4 GetVoxelInfo3(float3 voxelPosition)
{
	float4 info = VoxelGrid3.SampleLevel(gsamLinearWrap, voxelPosition, 0);
	return info;
}

// Returns the voxel information from grid 4
inline float4 GetVoxelInfo4(float3 voxelPosition)
{
	float4 info = VoxelGrid4.SampleLevel(gsamLinearWrap, voxelPosition, 0);
	return info;
}

// Returns the voxel information from grid 5
inline float4 GetVoxelInfo5(float3 voxelPosition)
{
	float4 info = VoxelGrid5.SampleLevel(gsamLinearWrap, voxelPosition, 0);
	return info;
}

inline float3 DiffuseConeTrace(float3 worldPosition, float3 coneDirection)
{
	float3 coneOrigin = worldPosition + (coneDirection * coneStep * ITERATIONS_0);

	float3 currentPosition = coneOrigin;
	float4 currentVoxelInfo = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float hitFound = 0.0f;

	// Sample voxel grid 1
	for (float i1 = 0.0f; i1 < ITERATIONS_1; i1 += 1.0f)
	{
		currentPosition += (coneStep * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo1(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}
	
	if (hitFound > 0.05f)
		return currentVoxelInfo.rgb;

	// Sample voxel grid 2
	for (float i2 = 0.0f; i2 < ITERATIONS_2; i2 += 1.0f)
	{
		currentPosition += (coneStep * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo2(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	if (hitFound > 0.05f)
		return currentVoxelInfo.rgb;
	
	// Sample voxel grid 3
	for (float i3 = 0.0f; i3 < ITERATIONS_3; i3 += 1.0f)
	{
		currentPosition += (coneStep * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo3(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	if (hitFound > 0.05f)
		return currentVoxelInfo.rgb;
	
	// Sample voxel grid 4
	for (float i4 = 0.0f; i4 < ITERATIONS_4; i4 += 1.0f)
	{
		currentPosition += (coneStep * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo4(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	if (hitFound > 0.05f)
		return currentVoxelInfo.rgb;
	
	// Sample voxel grid 5
	for (float i5 = 0.0f; i5 < ITERATIONS_5; i5 += 1.0f)
	{
		currentPosition += (coneStep * coneDirection);

		if (hitFound < 0.05f)
		{
			currentVoxelInfo = GetVoxelInfo5(GetVoxelPosition(currentPosition));
			hitFound = currentVoxelInfo.a;
		}
	}

	return currentVoxelInfo.rgb;
}