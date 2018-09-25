// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    DiffuseMetallicGBuffer  : register(t0);
Texture2D	 NormalRoughnessGBuffer  : register(t1);
Texture2D	 PositionDepthGBuffer	 : register(t2);
Texture2D	 ShadowPosHGBuffer		 : register(t3);
Texture2D	 ShadowMap				 : register(t4);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);
SamplerComparisonState gsamShadow	 : register(s2);

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
	float4 albedo = DiffuseMetallicGBuffer.Sample(gsamLinearWrap, pin.TexC);
	float metallic = albedo.a;
	float4 normal = NormalRoughnessGBuffer.Sample(gsamLinearWrap, pin.TexC);
	float roughness = normal.a;
	float4 position = PositionDepthGBuffer.Sample(gsamLinearWrap, pin.TexC);

	float3 N = normal.xyz;
	float3 V = normalize(gEyePosW - position.xyz);

	float3 L = normalize(-1.0f * gSunLightDirection.xyz);
	float3 H = normalize(V + L);

	float NdotV = abs(dot(N, V)) + 1e-5f; // avoid artifact
	float LdotH = saturate(dot(L ,H));
	float NdotH = saturate(dot(N ,H));
	float NdotL = saturate(dot(N ,L));

	// BRDF : Disney Diffuse + GGX Specular

	float energyBias = lerp(0.0f, 0.5f, roughness);
	float fd90 = energyBias + 2.0f * LdotH * LdotH * roughness ;
	float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);
	
	// Specular BRDF
	float3 F = FresnelSchlick(f0, fd90, LdotH);
	float Vis = SmithGGXCorrelated(NdotV, NdotL, roughness);
	float D = D_GGX(NdotH, roughness);
	float3 Fr = D * F * Vis / PI;

	// Diffuse BRDF
	float3 Fd = DiffuseDisneyNormalized(albedo.rgb, roughness * roughness, NdotV, NdotL, LdotH) / PI;

	Fd *= (1.0f - metallic);

	float3 directLight = ((Fd + Fr) * (gSunLightStrength.rgb * gSunLightStrength.a * NdotL));
	
	// Calculate shadow
    float4 shadowPosH = ShadowPosHGBuffer.Sample(gsamLinearWrap, pin.TexC);
	float shadow = CalculateShadow(shadowPosH, ShadowMap, gsamShadow);
	
	directLight *= (1.0f - shadow);

	return float4(directLight, 1.0f);
}