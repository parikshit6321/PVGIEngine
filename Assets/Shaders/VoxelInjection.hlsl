cbuffer cbSettings : register(b0)
{
	uint voxelResolution;
	float worldVolumeBoundary;
};

Texture2D lightingTexture           : register(t0);
Texture2D positionDepthTexture     	: register(t1);
RWTexture3D<float4> voxelGrid 		: register(u0);

// Function to get position of voxel in the grid
inline uint3 GetVoxelPosition (float3 worldPosition)
{
	float3 encodedPosition = worldPosition / worldVolumeBoundary;
	encodedPosition += float3(1.0f, 1.0f, 1.0f);
	encodedPosition /= 2.0f;
	uint3 voxelPosition = (uint3)(encodedPosition * voxelResolution);
	return voxelPosition;
}

[numthreads(1, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	// Color of the current voxel with lighting
	float3 lightingColor = lightingTexture[id.xy].rgb;

	// World space position
	float3 worldPosition = positionDepthTexture[id.xy].rgb;

	// Extract the pixel's depth
	float depth = positionDepthTexture[id.xy].a;

	uint3 voxelPosition = GetVoxelPosition(worldPosition);

	// Inject the current voxel's information into the grid
	if (((depth < voxelGrid[voxelPosition].a) && (depth > 0.1f)) || (voxelGrid[voxelPosition].a == 0.0f))
	{
		voxelGrid[voxelPosition] = float4(lightingColor, depth);
	}
}