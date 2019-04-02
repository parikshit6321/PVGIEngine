#define LUMA_THRESHOLD_FACTOR 0.02f // Higher = higher accuracy with higher flickering
#define LUMA_DEPTH_FACTOR 100.0f 	// Higher = lesser variation with depth
#define LUMA_FACTOR 1.9632107f

cbuffer cbSettings : register(b0)
{
	uint voxelResolution;
	float worldVolumeBoundary;
	uint rsmDownsample;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
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
};

Texture2D LightingTexture      		: register(t0);
Texture2D DepthMapTexture	   		: register(t1);

RWTexture3D<float4> voxelGrid0 		: register(u0);
RWTexture3D<float4> voxelGrid1 		: register(u1);
RWTexture3D<float4> voxelGrid2 		: register(u2);
RWTexture3D<float4> voxelGrid3 		: register(u3);
RWTexture3D<float4> voxelGrid4 		: register(u4);

// Get the world position from linear depth
inline float3 GetWorldPosition(float depth, uint2 id)
{
    float z = depth * 2.0f - 1.0f;

	float2 texCoord = float2(((float)id.x * gInvRenderTargetSize.x), ((float)id.y * gInvRenderTargetSize.y));

    float4 clipSpacePosition = float4(texCoord * 2.0f - 1.0f, z, 1.0f);
    float4 viewSpacePosition = mul(clipSpacePosition, gInvProj);

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    float4 worldSpacePosition = mul(viewSpacePosition, gInvView);

    return worldSpacePosition.xyz;
}

// Function to encode worldPosition in [0..1] range
inline float3 EncodePosition(float3 worldPosition)
{
	float3 encodedPosition = worldPosition / worldVolumeBoundary;
	encodedPosition += float3(1.0f, 1.0f, 1.0f);
	encodedPosition *= 0.5f;
	return encodedPosition;
}

// Function to get position of voxel in the grid
inline uint3 GetVoxelPosition(float3 encodedPosition, uint resolution)
{
	uint3 voxelPosition = (uint3)(encodedPosition * resolution);
	return voxelPosition;
}

// Function to get the luma value of the input color
inline float GetLuma(float3 inputColor)
{
	return ((inputColor.y * LUMA_FACTOR) + inputColor.x);
}

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	float4 lightingDepth = LightingTexture[id.xy];
	
	// Extract the pixel's depth
	float depth = DepthMapTexture[id.xy];

	// Only carry on with injection if voxels don't already have the most accurate data
	if (depth <= 0.1f)
		return;

	// World space position in [0..1] range
	float3 encodedPosition = EncodePosition(GetWorldPosition(depth, id.xy * rsmDownsample));
	
	// Color of the current voxel with lighting
	float3 lightingColor = lightingDepth.rgb;
	
	// Calculate depth based luma threshold (decreases with increasing depth)
	float lumaThreshold = LUMA_THRESHOLD_FACTOR * (1.0f / max(depth * LUMA_DEPTH_FACTOR, 0.1f));
	
	// Find the current pixel's luma
	float pixelLuma = GetLuma(lightingColor);
	
	// Enter data into the zeroith voxel grid
	uint3 voxelPosition = GetVoxelPosition(encodedPosition, voxelResolution);

	// Value to be injected into the voxel grids
	float4 injectionValue = float4(lightingColor, depth);
	
	// Inject the current voxel's information into the grid
	float4 currentVoxelInfo = voxelGrid0[voxelPosition];

	// Calculate difference between voxel and pixel luma
	float currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	float lumaDiff = saturate(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if ((currentVoxelInfo.a == 0.0f) || ((depth < currentVoxelInfo.a) && (lumaDiff < lumaThreshold)))
	{
		voxelGrid0[voxelPosition] = injectionValue;
	}
	
	// Enter data into the first voxel grid
	voxelPosition = GetVoxelPosition(encodedPosition, (voxelResolution >> 1));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid1[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = saturate(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if ((currentVoxelInfo.a == 0.0f) || ((depth < currentVoxelInfo.a) && (lumaDiff < lumaThreshold)))
	{
		voxelGrid1[voxelPosition] = injectionValue;
	}
	
	// Enter data into the second voxel grid
	voxelPosition = GetVoxelPosition(encodedPosition, (voxelResolution >> 2));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid2[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = saturate(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if ((currentVoxelInfo.a == 0.0f) || ((depth < currentVoxelInfo.a) && (lumaDiff < lumaThreshold)))
	{
		voxelGrid2[voxelPosition] = injectionValue;
	}
	
	// Enter data into the third voxel grid
	voxelPosition = GetVoxelPosition(encodedPosition, (voxelResolution >> 3));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid3[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = saturate(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if ((currentVoxelInfo.a == 0.0f) || ((depth < currentVoxelInfo.a) && (lumaDiff < lumaThreshold)))
	{
		voxelGrid3[voxelPosition] = injectionValue;
	}
	
	// Enter data into the fourth voxel grid
	voxelPosition = GetVoxelPosition(encodedPosition, (voxelResolution >> 4));

	// Inject the current voxel's information into the grid
	currentVoxelInfo = voxelGrid4[voxelPosition];
	
	// Calculate difference between voxel and pixel luma
	currentVoxelLuma = GetLuma(currentVoxelInfo.rgb);
	lumaDiff = saturate(currentVoxelLuma - pixelLuma);
	
	// Only inject if currently voxel is either 1. unoccupied or 2. of a lesser depth and passes luma test
	if ((currentVoxelInfo.a == 0.0f) || ((depth < currentVoxelInfo.a) && (lumaDiff < lumaThreshold)))
	{
		voxelGrid4[voxelPosition] = injectionValue;
	}
}