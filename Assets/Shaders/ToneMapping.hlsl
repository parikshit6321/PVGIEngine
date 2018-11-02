// Include structures and functions for lighting.
#include "ToneMappingUtil.hlsl"

Texture2D    MainTex  : register(t0);

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
	vout.PosH = float4(2.0f * vin.TexC.x - 1.0f, 1.0f - 2.0f * vin.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	return pow(float4(Uncharted2Tonemap(MainTex.Load(int3(pin.PosH.xy, 0)).rgb), 1.0f), GAMMA_FACTOR);
}