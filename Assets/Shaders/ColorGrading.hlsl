#define HALF_PIXEL_X 0.001953125f
#define HALF_PIXEL_Y 0.03125f
#define DIMENSION 16.0f
#define ONE_DIV_DIMENSION 0.0625f
#define DIMENSION_MINUS_1 15.0f
#define DIMENSION_MINUS_1_DIV_DIMENSION 0.9375f

Texture2D    MainTex  : register(t0);
Texture2D	 UserLUT  : register(t1);

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
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f*vin.TexC.x - 1.0f, 1.0f - 2.0f*vin.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 inputColor = MainTex.Load(int3(pin.PosH.xy, 0));

	float blueCell = inputColor.b * DIMENSION_MINUS_1;
	
	float rComponentOffset = ((inputColor.r * ONE_DIV_DIMENSION) * DIMENSION_MINUS_1_DIV_DIMENSION) + HALF_PIXEL_X;
	float gComponentOffset = (inputColor.g * DIMENSION_MINUS_1_DIV_DIMENSION) + HALF_PIXEL_Y;

	float2 positionInLUTLower = float2((floor(blueCell) * ONE_DIV_DIMENSION) + rComponentOffset, gComponentOffset);
	float2 positionInLUTUpper = float2((ceil(blueCell) * ONE_DIV_DIMENSION) + rComponentOffset, gComponentOffset);

	float4 gradedColorLower = UserLUT.Sample(gsamLinearWrap, positionInLUTLower);
	float4 gradedColorUpper = UserLUT.Sample(gsamLinearWrap, positionInLUTUpper);

	float4 gradedColor = lerp(gradedColorLower, gradedColorUpper, frac(blueCell));

	float4 result = lerp(inputColor, gradedColor, userLUTContribution);

	return result;
}