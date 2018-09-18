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
	float depth = position.a;

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

	float3 kS = F;
	float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS);

	kD *= (1.0f - metallic);

	float3 diffuse = kD * DiffuseBurley(albedo.rgb, roughness, NdotV, NdotL, VdotH);

	float3 indirectLight = (diffuse * CalculateIndirectLighting(position.xyz, normalize(N)));
	
	float3 directLight = MainTex.Sample(gsamLinearWrap, pin.TexC).rgb;
	float3 finalResult = directLight + indirectLight;
	
	return float4(finalResult, 1.0f);
}