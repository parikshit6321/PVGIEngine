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
	int3 TexC = int3(pin.PosH.xy, 0);

	float3 V = normalize(gEyePosW - PositionDepthGBuffer.Load(TexC).xyz);
	
	float4 normal = NormalRoughnessGBuffer.Load(TexC);
	float4 albedo = DiffuseMetallicGBuffer.Load(TexC);
	float4 shadowPosH = ShadowPosHGBuffer.Load(TexC);
	
	float3 L = normalize(gSunLightDirection.xyz);
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L ,H));
	
	float3 N = normal.xyz;
	float linearRoughness = normal.a * normal.a;
	float NdotV = abs(dot(N, V)) + 1e-5f; // avoid artifact
	float NdotL = saturate(dot(N ,L));
	float NdotH = saturate(dot(N ,H));
	
	float fd90 = linearRoughness * (0.5f + (2.0f * LdotH * LdotH));
	
	// Specular BRDF (GGX)
	float Vis = SmithGGXCorrelated(NdotV, NdotL, linearRoughness);
	float D = D_GGX(NdotH, linearRoughness);
	
	float3 specularF0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, albedo.a);
	
	// Diffuse BRDF (Lambertian)
	float3 Fd = LambertianDiffuse(albedo.rgb) * (1.0f - albedo.a);
	
	// Calculate shadow
	float shadow = CalculateShadow(shadowPosH, ShadowMap, gsamShadow);

	float3 F = FresnelSchlick(specularF0, fd90, LdotH);
	float3 Fr = D * Vis * PI_INVERSE * F;
	
	float3 directLight = ((Fd + Fr) * (gSunLightStrength.rgb * gSunLightStrength.a * NdotL * (1.0f - shadow)));
	
	return float4(directLight, 1.0f);
}