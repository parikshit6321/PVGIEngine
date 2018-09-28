// Include structures and functions for lighting.
#include "NormalMappingUtil.hlsl"

Texture2D    gDiffuseOpacityMap		 : register(t0);
Texture2D	 gNormalRoughnessMap	 : register(t1);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
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

cbuffer cbMaterial : register(b2)
{
	float4		gMetallic;
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
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION0;
	float4 ShadowPosH : POSITION1;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentW : TANGENT;
};

struct PixelOut
{
	float4 DiffuseMetallicGBuffer	: SV_TARGET0;
	float4 NormalRoughnessGBuffer	: SV_TARGET1;
	float4 PositionDepthGBuffer		: SV_TARGET2;
	float4 ShadowPosHGBuffer	 	: SV_TARGET3;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

	vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);

	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);

	// Output texture coordinates for interpolation across triangle.
	vout.TexC = vin.TexC;
	
	vout.ShadowPosH = mul(posW, gShadowTransform);

	return vout;
}

PixelOut PS(VertexOut pin)
{
	PixelOut output;

	float4 albedo = pow(gDiffuseOpacityMap.Sample(gsamAnisotropicWrap, pin.TexC), 2.2f);
	float opacity = albedo.a;

	// Discard the current fragment if it's not opaque.
	clip(opacity - 0.1f);
		
	float4 normalT = gNormalRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC);
	float roughness = normalT.a;
	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

	float3 N = NormalSampleToWorldSpace(normalT.xyz, pin.NormalW, pin.TangentW);

	float depth = pin.PosH.z / pin.PosH.w;
	pin.ShadowPosH.xyz /= pin.ShadowPosH.w;
	
	output.DiffuseMetallicGBuffer = float4(albedo.rgb, gMetallic.r);
	output.NormalRoughnessGBuffer = float4(N, roughness);
	output.PositionDepthGBuffer = float4(pin.PosW, depth);
	output.ShadowPosHGBuffer = float4(pin.ShadowPosH);
	
	return output;
}