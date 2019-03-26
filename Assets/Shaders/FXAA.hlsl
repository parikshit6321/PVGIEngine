#include "FXAAUtil.hlsl"

Texture2D Input				 	: register(t0);
RWTexture2D<float4> Output		: register(u0);

SamplerState gsamLinearWrap		: register(s0);

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
};

inline float4 FXAA(int2 texC)
{
	// Get the input pixel color
	float4 inputColor = Input[texC];
	
	// Find luma at the current pixel
	float lumaCenter = GetColorLuma(inputColor.rgb);

	// Find luma at the four direct neighbouring pixels
	float lumaUp = GetColorLuma(Input[texC + int2(0, 1)].rgb);
	float lumaDown = GetColorLuma(Input[texC + int2(0, -1)].rgb);
	float lumaLeft = GetColorLuma(Input[texC + int2(-1, 0)].rgb);
	float lumaRight = GetColorLuma(Input[texC + int2(1, 0)].rgb);
	
	// Find maximum and minimum luma 
	float lumaMin = min(lumaCenter, min(min(lumaUp, lumaDown), min(lumaLeft, lumaRight)));
	float lumaMax = max(lumaCenter, max(max(lumaUp, lumaDown), max(lumaLeft, lumaRight)));
	
	// Compute the range of luma of the direct neighbouring pixels
	float lumaRange = lumaMax - lumaMin;
	
	// If the luma variation is lower than a certain threshold or we're in 
	// a really dark area, we are not on an edge (don't perform any AA)
	if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
	{
		return inputColor;
	}
	
	// Find luma of the four corner pixels
	float lumaDownLeft = GetColorLuma(Input[texC + int2(-1, -1)].rgb);
	float lumaUpRight = GetColorLuma(Input[texC + int2(1, 1)].rgb);
	float lumaUpLeft = GetColorLuma(Input[texC + int2(-1, 1)].rgb);
	float lumaDownRight = GetColorLuma(Input[texC + int2(1, -1)].rgb);

	// Combine the four edges lumas
	float lumaDownUp = lumaDown + lumaUp;
	float lumaLeftRight = lumaLeft + lumaRight;

	// Combine the four corner lumas
	float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
	float lumaDownCorners = lumaDownLeft + lumaDownRight;
	float lumaRightCorners = lumaDownRight + lumaUpRight;
	float lumaUpCorners = lumaUpRight + lumaUpLeft;
	
	// Compute an estimation of the gradients along horizontal and vertical axes
	float gradientHorizontal = abs((-2.0f * lumaLeft) + lumaLeftCorners) + abs((-2.0f * lumaCenter) + lumaDownUp ) * 2.0f + abs((-2.0f * lumaRight) + lumaRightCorners);
	float gradientVertical = abs((-2.0f * lumaUp) + lumaUpCorners) + abs((-2.0f * lumaCenter) + lumaLeftRight) * 2.0f + abs((-2.0f * lumaDown) + lumaDownCorners);

	// Is the local edge horizontal or vertical
	bool isHorizontal = (gradientHorizontal >= gradientVertical);
	
	// Select the two neighbouring pixel lumas in the opposite direction to local edge
	float luma1 = isHorizontal ? lumaDown : lumaLeft;
	float luma2 = isHorizontal ? lumaUp : lumaRight;
	
	// Compute gradients in this direction
	float gradient1 = abs(luma1 - lumaCenter);
	float gradient2 = abs(luma2 - lumaCenter);

	// Find out which direction is steepest
	bool is1Steepest = (gradient1 >= gradient2);

	// Gradient in the corresponding direction (normalized)
	float gradientScaled = 0.25f * max(gradient1, gradient2);
	
	// Choose the step size (one pixel) according to the edge direction
	float stepLength = isHorizontal ? (gInvRenderTargetSize.y * 0.5f) : (gInvRenderTargetSize.x * 0.5f);

	// Average luma in the correct direction
	float lumaLocalAverage;

	// Switch the direction if true and compute average
	if(is1Steepest){
		stepLength = - stepLength;
		lumaLocalAverage = 0.5f * (luma1 + lumaCenter);
	} else {
		lumaLocalAverage = 0.5f * (luma2 + lumaCenter);
	}

	// Shift UV in the correct direction by half a pixel
	float2 currentUV = float2((texC.x * gInvRenderTargetSize.x), (texC.y * gInvRenderTargetSize.y));
	if(isHorizontal){
		currentUV.y += stepLength;
	} else {
		currentUV.x += stepLength;
	}

	// Compute offset (for each iteration step) in the right direction
	float2 offset = isHorizontal ? float2(gInvRenderTargetSize.x, 0.0f) : float2(0.0f ,gInvRenderTargetSize.y);
	
	// Compute UVs to explore on each side of the edge, orthogonally.
	// The QUALITY array allows us to step faster
	float2 uv1 = currentUV - offset;
	float2 uv2 = currentUV + offset;

	// Read the lumas at both current extremities of the exploration segment,
	// and compute the delta with respect to the local average luma.
	float lumaEnd1 = GetColorLuma(Input.SampleLevel(gsamLinearWrap, uv1, 0).rgb) - lumaLocalAverage;
	float lumaEnd2 = GetColorLuma(Input.SampleLevel(gsamLinearWrap, uv2, 0).rgb) - lumaLocalAverage;
	
	// If the luma deltas at the current extremities are larger 
	// than the local gradient, we have reached the side of the edge
	bool reached1 = abs(lumaEnd1) >= gradientScaled;
	bool reached2 = abs(lumaEnd2) >= gradientScaled;
	bool reachedBoth = reached1 && reached2;

	// If the side is not reached, we continue to explore in this direction
	if(!reached1){
		uv1 -= offset;
	}
	if(!reached2){
		uv2 += offset;
	}

	// If both sides have not been reached, continue to explore
	if(!reachedBoth){

		for(int i = 2; i < ITERATIONS; ++i){
			
			// If needed, read luma in first direction and compute delta
			lumaEnd1 = (reached1) ? lumaEnd1 : (GetColorLuma(Input.SampleLevel(gsamLinearWrap, uv1, 0).rgb) - lumaLocalAverage);
			
			// If needed, read luma in opposite direction and compute delta
			lumaEnd2 = (reached2) ? lumaEnd2 : (GetColorLuma(Input.SampleLevel(gsamLinearWrap, uv2, 0).rgb) - lumaLocalAverage);
			
			// If the luma deltas at the current extremities are larger 
			// than the local gradient, we have reached the side of the edge
			reached1 = abs(lumaEnd1) >= gradientScaled;
			reached2 = abs(lumaEnd2) >= gradientScaled;
			reachedBoth = (reached1 && reached2);

			// If the side is not reached, we continue to explore in this direction with a variable quality
			if(!reached1){
				uv1 -= (offset * QUALITY[i]);
			}
			if(!reached2){
				uv2 += (offset * QUALITY[i]);
			}

			// If both sides have been reached, stop the exploration
			if(reachedBoth){ 
				break;
			}
		}
	}
	
	// Compute the distance to each extremity of the edge
	float distance1 = isHorizontal ? ((texC.x * gInvRenderTargetSize.x) - uv1.x) : ((texC.y * gInvRenderTargetSize.y) - uv1.y);
	float distance2 = isHorizontal ? (uv2.x - (texC.x * gInvRenderTargetSize.x)) : (uv2.y - (texC.y * gInvRenderTargetSize.y));

	// Find the direction in which the extremity of the edge is closer
	bool isDirection1 = distance1 < distance2;
	float distanceFinal = min(distance1, distance2);

	// Length of the edge
	float edgeThickness = (distance1 + distance2);

	// UV Offset : read in the direction of the closest side of the edge
	float pixelOffset = (-1.0f *  distanceFinal) / edgeThickness + 0.5f;
	
	// Is the luma at center smaller than the local average
	bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
	
	// If the luma at center is smaller than at it's neighbour, the delta luma at each end
	// should be positive (same variation) in the direction of the closer side of the edge
	bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;

	// If the luma variation is incorrect, do not offset
	float finalOffset = correctVariation ? pixelOffset : 0.0f;

	// Sub-pixel shifting
	// Full-weighted average of the luma over the 3x3 neighbourhood
	float lumaAverage = ONE_DIV_TWELVE * ((2.0f * (lumaDownUp + lumaLeftRight)) + lumaLeftCorners + lumaRightCorners);
	
	// Ratio of the delta between the global average and the center luma, over the luma range in the 3x3 neighbourhood
	float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0f, 1.0f);
	float subPixelOffset2 = (-2.0f * subPixelOffset1 + 3.0f) * subPixelOffset1 * subPixelOffset1;
	
	// Compute a sub-pixel offset based on this delta
	float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

	// Pick the biggest of the two offsets
	finalOffset = max(finalOffset, subPixelOffsetFinal);
	
	// Compute the final UV coordinates.
	float2 finalUV = float2((texC.x * gInvRenderTargetSize.x), (texC.y * gInvRenderTargetSize.y));
	
	if(isHorizontal){
		finalUV.y += (finalOffset * stepLength);
	} else {
		finalUV.x += (finalOffset * stepLength);
	}

	// Sample the input texture using the new offset
	float4 finalColor = Input.SampleLevel(gsamLinearWrap, finalUV, 0);
	
	return finalColor;
}

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	Output[id.xy] = FXAA(id.xy);
}