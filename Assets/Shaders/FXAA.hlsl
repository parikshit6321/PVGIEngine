// Include structures and functions for lighting.
#include "FXAAUtil.hlsl"

Texture2D    MainTex				 : register(t0);

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
};

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentU : TANGENT;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.TexC = vin.TexC;

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float3 pixelColor = MainTex.Sample(gsamLinearWrap, pin.TexC).rgb;
	
	float lumaCenter = GetColorLuma(pixelColor);
	float lumaUp = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(0.0f, gInvRenderTargetSize.y)).rgb);
	float lumaDown = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(0.0f, -gInvRenderTargetSize.y)).rgb);
	float lumaLeft = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(-gInvRenderTargetSize.x, 0.0f)).rgb);
	float lumaRight = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(gInvRenderTargetSize.x, 0.0f)).rgb);
	
	float lumaMin = min(lumaCenter, min(min(lumaUp, lumaDown), min(lumaLeft, lumaRight)));
	float lumaMax = max(lumaCenter, max(max(lumaUp, lumaDown), max(lumaLeft, lumaRight)));
	
	float lumaRange = lumaMax - lumaMin;
	
	if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
	{
		return float4(pixelColor, 1.0f);
	}
	
	float lumaDownLeft = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(-gInvRenderTargetSize.x, -gInvRenderTargetSize.y)).rgb);
	float lumaUpRight = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(gInvRenderTargetSize.x, gInvRenderTargetSize.y)).rgb);
	float lumaUpLeft = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(-gInvRenderTargetSize.x, gInvRenderTargetSize.y)).rgb);
	float lumaDownRight = GetColorLuma(MainTex.Sample(gsamLinearWrap, pin.TexC + float2(gInvRenderTargetSize.x, -gInvRenderTargetSize.y)).rgb);

	float lumaDownUp = lumaDown + lumaUp;
	float lumaLeftRight = lumaLeft + lumaRight;

	float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
	float lumaDownCorners = lumaDownLeft + lumaDownRight;
	float lumaRightCorners = lumaDownRight + lumaUpRight;
	float lumaUpCorners = lumaUpRight + lumaUpLeft;

	float gradientHorizontal =  abs((-2.0 * lumaLeft) + lumaLeftCorners)  + abs((-2.0 * lumaCenter) + lumaDownUp ) * 2.0    + abs((-2.0 * lumaRight) + lumaRightCorners);
	float gradientVertical =    abs((-2.0 * lumaUp) + lumaUpCorners)      + abs((-2.0 * lumaCenter) + lumaLeftRight) * 2.0  + abs((-2.0 * lumaDown) + lumaDownCorners);

	bool isHorizontal = (gradientHorizontal >= gradientVertical);
	
	float luma1 = isHorizontal ? lumaDown : lumaLeft;
	float luma2 = isHorizontal ? lumaUp : lumaRight;
	
	float gradient1 = luma1 - lumaCenter;
	float gradient2 = luma2 - lumaCenter;

	bool is1Steepest = (abs(gradient1) >= abs(gradient2));

	float gradientScaled = 0.25f * max(abs(gradient1), abs(gradient2));
	
	float stepLength = isHorizontal ? gInvRenderTargetSize.y : gInvRenderTargetSize.x;

	float lumaLocalAverage = 0.0f;

	if(is1Steepest){
		stepLength = - stepLength;
		lumaLocalAverage = 0.5f * (luma1 + lumaCenter);
	} else {
		lumaLocalAverage = 0.5f * (luma2 + lumaCenter);
	}

	float2 currentUV = pin.TexC;
	
	if(isHorizontal){
		currentUV.y += (stepLength * 0.5f);
	} else {
		currentUV.x += (stepLength * 0.5f);
	}
	
	float2 offset = isHorizontal ? float2(gInvRenderTargetSize.x, 0.0f) : float2(0.0f ,gInvRenderTargetSize.y);
	
	float2 uv1 = currentUV - offset;
	float2 uv2 = currentUV + offset;

	// Read the lumas at both current extremities of the exploration segment, and compute the delta wrt to the local average luma.
	float lumaEnd1 = GetColorLuma(MainTex.Sample(gsamLinearWrap, uv1).rgb);
	float lumaEnd2 = GetColorLuma(MainTex.Sample(gsamLinearWrap, uv2).rgb);
	
	lumaEnd1 -= lumaLocalAverage;
	lumaEnd2 -= lumaLocalAverage;

	bool reached1 = abs(lumaEnd1) >= gradientScaled;
	bool reached2 = abs(lumaEnd2) >= gradientScaled;
	bool reachedBoth = reached1 && reached2;

	if(!reached1){
		uv1 -= offset;
	}
	if(!reached2){
		uv2 += offset;
	}

	if(!reachedBoth){

		for(int i = 2; i < ITERATIONS; ++i){
			
			if(!reached1){
				lumaEnd1 = GetColorLuma(MainTex.Sample(gsamLinearWrap, uv1).rgb);
				lumaEnd1 = lumaEnd1 - lumaLocalAverage;
			}
			
			if(!reached2){
				lumaEnd2 = GetColorLuma(MainTex.Sample(gsamLinearWrap, uv1).rgb);
				lumaEnd2 = lumaEnd2 - lumaLocalAverage;
			}
			
			reached1 = abs(lumaEnd1) >= gradientScaled;
			reached2 = abs(lumaEnd2) >= gradientScaled;
			reachedBoth = reached1 && reached2;

			if(!reached1){
				uv1 -= offset * QUALITY[i];
			}
			if(!reached2){
				uv2 += offset * QUALITY[i];
			}

			if(reachedBoth){ 
				break;
			}
		}
	}
	
	float distance1 = isHorizontal ? (pin.TexC.x - uv1.x) : (pin.TexC.y - uv1.y);
	float distance2 = isHorizontal ? (uv2.x - pin.TexC.x) : (uv2.y - pin.TexC.y);

	bool isDirection1 = distance1 < distance2;
	float distanceFinal = min(distance1, distance2);

	float edgeThickness = (distance1 + distance2);

	float pixelOffset = - distanceFinal / edgeThickness + 0.5f;
	
	bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
	bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;

	float finalOffset = correctVariation ? pixelOffset : 0.0f;
	
	float lumaAverage = (1.0f / 12.0f) * ((2.0f * (lumaDownUp + lumaLeftRight)) + lumaLeftCorners + lumaRightCorners);
	
	float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0f, 1.0f);
	float subPixelOffset2 = (-2.0f * subPixelOffset1 + 3.0f) * subPixelOffset1 * subPixelOffset1;
	
	float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

	finalOffset = max(finalOffset,subPixelOffsetFinal);
	
	// Compute the final UV coordinates.
	float2 finalUV = pin.TexC;
	
	if(isHorizontal){
		finalUV.y += (finalOffset * stepLength);
	} else {
		finalUV.x += (finalOffset * stepLength);
	}

	float3 finalColor = MainTex.Sample(gsamLinearWrap, finalUV).rgb;
	
	return float4(finalColor, 1.0f);
}