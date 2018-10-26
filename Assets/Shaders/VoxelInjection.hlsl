#define LUMA_THRESHOLD_FACTOR 0.1f
#define LUMA_DEPTH_FACTOR 100.0f

cbuffer cbSettings : register(b0)
{
	uint voxelResolution;
	float worldVolumeBoundary;
	uint rsmDownsample;
};

Texture2D lightingTexture           : register(t0);
Texture2D positionDepthTexture     	: register(t1);

RWTexture3D<float4> voxelGrid0 		: register(u0);
RWTexture3D<float4> voxelGrid1 		: register(u1);
RWTexture3D<float4> voxelGrid2 		: register(u2);
RWTexture3D<float4> voxelGrid3 		: register(u3);
RWTexture3D<float4> voxelGrid4 		: register(u4);
RWTexture3D<float4> voxelGrid5 		: register(u5);

// Function to get position of voxel in the grid
inline uint3 GetVoxelPosition (float3 worldPosition, uint resolution)
{
	float3 encodedPosition = worldPosition / worldVolumeBoundary;
	encodedPosition += float3(1.0f, 1.0f, 1.0f);
	encodedPosition *= 0.5f;
	uint3 voxelPosition = (uint3)(encodedPosition * resolution);
	return voxelPosition;
}

// Function to get the luma value of the input color
inline float GetLuma(float3 inputColor)
{
	return ((0.299f * inputColor.r) + (0.587f * inputColor.g) + (0.114f * inputColor.b));
}

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	// Color of the current voxel with lighting
	float3 lightingColor = lightingTexture[id.xy * rsmDownsample].rgb;

	// World space position
	float3 worldPosition = positionDepthTexture[id.xy * rsmDownsample].rgb;

	// Extract the pixel's depth
	float depth = positionDepthTexture[id.xy].a;

	// Calculate depth based luma threshold (decreases with increasing depth)
	float lumaThreshold = LUMA_THRESHOLD_FACTOR * (1.0f / max(depth * LUMA_DEPTH_FACTOR, 0.1f));
	
	// Find the current pixel's luma
	float pixelLuma = GetLuma(lightingColor);
	
	// Enter data into the zeroith voxel grid
	uint3 voxelPosition = GetVoxelPosition(worldPosition, voxelResolution);

	// Inject the current voxel's information into the grid
	float4 currentVoxelInfo = voxelGrid0[voxelPosition];

	// Calculate difference between voxel and pixel luma
	float currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	float lumaDiff = abs(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if (((depth < currentVoxelInfo.a) && (depth > 0.1f) && (lumaDiff < lumaThreshold)) || (currentVoxelInfo.a == 0.0f))
	{
		voxelGrid0[voxelPosition] = float4(lightingColor, depth);
	}
	
	// Enter data into the first voxel grid
	voxelPosition = GetVoxelPosition(worldPosition, (voxelResolution / 2));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid1[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = abs(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if (((depth < currentVoxelInfo.a) && (depth > 0.1f) && (lumaDiff < lumaThreshold)) || (currentVoxelInfo.a == 0.0f))
	{
		voxelGrid1[voxelPosition] = float4(lightingColor, depth);
	}
	
	// Enter data into the second voxel grid
	voxelPosition = GetVoxelPosition(worldPosition, (voxelResolution / 4));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid2[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = abs(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if (((depth < currentVoxelInfo.a) && (depth > 0.1f) && (lumaDiff < lumaThreshold)) || (currentVoxelInfo.a == 0.0f))
	{
		voxelGrid2[voxelPosition] = float4(lightingColor, depth);
	}
	
	// Enter data into the third voxel grid
	voxelPosition = GetVoxelPosition(worldPosition, (voxelResolution / 8));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid3[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = abs(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if (((depth < currentVoxelInfo.a) && (depth > 0.1f) && (lumaDiff < lumaThreshold)) || (currentVoxelInfo.a == 0.0f))
	{
		voxelGrid3[voxelPosition] = float4(lightingColor, depth);
	}
	
	// Enter data into the fourth voxel grid
	voxelPosition = GetVoxelPosition(worldPosition, (voxelResolution / 16));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid4[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = abs(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if (((depth < currentVoxelInfo.a) && (depth > 0.1f) && (lumaDiff < lumaThreshold)) || (currentVoxelInfo.a == 0.0f))
	{
		voxelGrid4[voxelPosition] = float4(lightingColor, depth);
	}
	
	// Enter data into the fifth voxel grid
	voxelPosition = GetVoxelPosition(worldPosition, (voxelResolution / 32));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid5[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = abs(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if (((depth < currentVoxelInfo.a) && (depth > 0.1f) && (lumaDiff < lumaThreshold)) || (currentVoxelInfo.a == 0.0f))
	{
		voxelGrid5[voxelPosition] = float4(lightingColor, depth);
	}
}