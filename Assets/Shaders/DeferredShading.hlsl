// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    DiffuseMetallicGBuffer  : register(t0);
Texture2D	 NormalRoughnessGBuffer  : register(t1);
Texture2D	 PositionDepthGBuffer	 : register(t2);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);
SamplerState gsamShadow				 : register(s2);

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
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.TexC = vin.TexC;

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 albedo = DiffuseMetallicGBuffer.Sample(gsamAnisotropicWrap, pin.TexC);
	float metallic = albedo.a;
	float4 normal = NormalRoughnessGBuffer.Sample(gsamAnisotropicWrap, pin.TexC);
	float roughness = normal.a;
	float4 position = PositionDepthGBuffer.Sample(gsamAnisotropicWrap, pin.TexC);
	
	float3 skyColor = float3(0.80f, 0.941f, 1.0f);

	float3 N = normal.xyz;
	float3 V = normalize(gEyePosW - position.xyz);

	float3 L = normalize(-1.0f * gSunLightDirection.xyz);
	float3 H = normalize(V + L);

	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float VdotH = max(dot(V, H), 0.0f);
	float NdotH = max(dot(N, H), 0.0f);

	// BRDF : Disney Diffuse + GGX Specular

	// Calculate Fresnel effect
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo.rgb, metallic);
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

	kD *= (1.0f - metallic);

	float3 diffuse = kD * DiffuseBurley(albedo.rgb, roughness, NdotV, NdotL, VdotH);

	float3 directLight = ((diffuse + specular) * (gSunLightStrength.rgb * NdotL));

	return float4(directLight, 1.0f);
}