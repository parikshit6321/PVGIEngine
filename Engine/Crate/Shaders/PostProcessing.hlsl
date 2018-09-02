//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    gDiffuseOpacityMap : register(t0);
Texture2D	 gNormalRoughnessMap : register(t1);

SamplerState gsamAnisotropicWrap  : register(s4);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
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
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gSunLightStrength;
	float4 gSunLightDirection;
};

cbuffer cbMaterial : register(b2)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float  gMetallic;
	float4x4 gMatTransform;
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
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentW : TANGENT;
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

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 albedo = gDiffuseOpacityMap.Sample(gsamAnisotropicWrap, pin.TexC);
	float opacity = albedo.a;

	// Discard the current fragment if it's not opaque.
	if (opacity < 0.1f)
		discard;

	float4 normalT = gNormalRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC);
	float roughness = normalT.a;
	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

	float3 N = NormalSampleToWorldSpace(normalT.xyz, pin.NormalW, pin.TangentW);
	float3 V = normalize(gEyePosW - pin.PosW);

	float3 L = normalize(-1.0f * gSunLightDirection.xyz);
	float3 H = normalize(V + L);

	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float VdotH = max(dot(V, H), 0.0f);
	float NdotH = max(dot(N, H), 0.0f);

	// BRDF : Disney Diffuse + GGX Specular

	// Calculate Fresnel effect
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo.rgb, gMetallic);
	float3 F = FresnelSchlick(VdotH, F0);

	// Calculate Normal distribution function
	float NDF = DistributionGGX(NdotH, roughness);

	// Calculate Geometry function
	float G = GeometrySmith(NdotV, NdotL, roughness);

	float3 numerator = (F * NDF * G);
	float denominator = (4.0f * NdotV * NdotL);
	float3 specular = numerator / max(denominator, 0.001f);

	float3 kS = F;
	float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS);

	kD *= (1.0f - gMetallic);

	float3 diffuse = kD * DiffuseBurley(albedo.rgb, roughness, NdotV, NdotL, VdotH);
	
	float3 directLight = ((diffuse + specular) * (gSunLightStrength.rgb * NdotL));

	// ACES Tonemapping
	directLight.rgb = ACESFitted(directLight.rgb);

	// Gamma Correction
	float gc = (1.0f / 2.2f);
	directLight.rgb = pow(directLight.rgb, float3(gc, gc, gc));

	return float4(directLight, 1.0f);
}