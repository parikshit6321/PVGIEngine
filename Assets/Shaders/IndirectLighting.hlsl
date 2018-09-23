// Include structures and functions for lighting.
#include "LightingUtil.hlsl"
#include "ConeTracingUtil.hlsl"

Texture2D    DiffuseMetallicGBuffer  : register(t0);
Texture2D	 NormalRoughnessGBuffer  : register(t1);
Texture2D	 PositionDepthGBuffer	 : register(t2);
Texture2D	 ShadowPosHGBuffer		 : register(t3);
Texture2D	 MainTex				 : register(t4);

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
	float4 albedo = DiffuseMetallicGBuffer.Sample(gsamAnisotropicWrap, pin.TexC);
	float metallic = albedo.a;
	float4 normal = NormalRoughnessGBuffer.Sample(gsamAnisotropicWrap, pin.TexC);
	float roughness = normal.a;
	float4 position = PositionDepthGBuffer.Sample(gsamAnisotropicWrap, pin.TexC);

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
	float3 f0 = lerp(float3(0.05f, 0.05f, 0.05f), albedo.rgb, metallic);
	
	// Diffuse BRDF
	float3 Fd = DiffuseDisneyNormalized(albedo.rgb, roughness * roughness, NdotV, NdotL, LdotH) / PI;
	
	Fd *= (1.0f - metallic);
	
	float3 indirectLight = (Fd * CalculateIndirectLighting(position.xyz, normalize(N)));
	
	float3 directLight = MainTex.Sample(gsamLinearWrap, pin.TexC).rgb;
	float3 finalResult = directLight + indirectLight;
	
	return float4(finalResult, 1.0f);
}