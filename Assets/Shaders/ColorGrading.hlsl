#define HALF_PIXEL_X 0.001953125f
#define HALF_PIXEL_Y 0.03125f
#define DIMENSION 16.0f
#define ONE_DIV_DIMENSION 0.0625f
#define DIMENSION_MINUS_1 15.0f
#define DIMENSION_MINUS_1_DIV_DIMENSION 0.9375f

Texture2D Input  			: register(t0);
Texture2D UserLUT  			: register(t1);

RWTexture2D<float4> Output	: register(u0);

SamplerState gsamLinearWrap	: register(s0);

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

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	float4 inputColor = Input[id.xy];

	float blueCell = inputColor.b * DIMENSION_MINUS_1;
	
	float rComponentOffset = ((inputColor.r * ONE_DIV_DIMENSION) * DIMENSION_MINUS_1_DIV_DIMENSION) + HALF_PIXEL_X;
	float gComponentOffset = (inputColor.g * DIMENSION_MINUS_1_DIV_DIMENSION) + HALF_PIXEL_Y;

	float2 positionInLUTLower = float2((floor(blueCell) * ONE_DIV_DIMENSION) + rComponentOffset, gComponentOffset);
	float2 positionInLUTUpper = float2((ceil(blueCell) * ONE_DIV_DIMENSION) + rComponentOffset, gComponentOffset);

	float4 gradedColorLower = UserLUT.SampleLevel(gsamLinearWrap, positionInLUTLower, 0);
	float4 gradedColorUpper = UserLUT.SampleLevel(gsamLinearWrap, positionInLUTUpper, 0);

	float4 gradedColor = lerp(gradedColorLower, gradedColorUpper, frac(blueCell));

	Output[id.xy] = lerp(inputColor, gradedColor, userLUTContribution);
}