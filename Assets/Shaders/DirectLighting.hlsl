// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    gDiffuseOpacityMap		 : register(t0);
Texture2D	 gNormalRoughnessMap	 : register(t1);
Texture2D	 ShadowMap				 : register(t2);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);
SamplerComparisonState gsamShadow	 : register(s2);

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
	float4 PosW    : POSITION0;
	float4 ShadowPosH : POSITION1;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentW : TANGENT;
};

struct PixelOut
{
	float4 DirectLightingDepth 		: SV_TARGET0;
	float4 DiffuseMetallicGBuffer	: SV_TARGET1;
	float4 NormalRoughnessGBuffer	: SV_TARGET2;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// Transform to world space.
	vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld);
	
	// Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

	vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);

	// Transform to homogeneous clip space.
	vout.PosH = mul(vout.PosW, gViewProj);

	// Output texture coordinates for interpolation across triangle.
	vout.TexC = vin.TexC;
	
	vout.ShadowPosH = mul(vout.PosW, gShadowTransform);

	return vout;
}

PixelOut PS(VertexOut pin)
{
	PixelOut output;

	// Sample the diffuse + opacity texture 
	float4 albedo = pow(gDiffuseOpacityMap.Sample(gsamAnisotropicWrap, pin.TexC), 2.2f);
	
	// Discard the current fragment if it's not opaque.
	clip(albedo.a - 0.1f);
	
	// Sample the normal + roughness texture
	float4 pixelNormal = gNormalRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC);

	// Store the global metallic flag in alpha channel
	albedo.a = gMetallic.r;
	
	// Do the perspective divide
	pin.ShadowPosH.xyz /= pin.ShadowPosH.w;
	
	// Build orthonormal basis.
	float3 N = normalize(pin.NormalW);
	float3 T = normalize(pin.TangentW - (dot(pin.TangentW, N) * N));
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);
	
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = ((2.0f * pixelNormal.xyz) - 1.0f);

	// Transform from tangent space to world space.
	pixelNormal.xyz = normalize(mul(normalT, TBN));

	float3 V = normalize(gEyePosW - pin.PosW.xyz);
	float3 L = normalize(gSunLightDirection.xyz);
	float3 H = normalize(V + L);
	float LdotH = saturate(dot(L ,H));
	
	N = pixelNormal.xyz;
	float linearRoughness = pixelNormal.a * pixelNormal.a;
	float NdotV = abs(dot(N, V)) + 1e-5f; // avoid artifact
	float NdotL = saturate(dot(N ,L));
	float NdotH = saturate(dot(N ,H));
	
	float fd90 = linearRoughness * (0.5f + (2.0f * LdotH * LdotH));
	
	// Specular BRDF (GGX)
	float Vis = SmithGGXCorrelated(NdotV, NdotL, linearRoughness);
	float D = D_GGX(NdotH, linearRoughness);
	
	float3 specularF0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, albedo.a);
	
	float3 F = FresnelSchlick(specularF0, fd90, LdotH);
	float3 Fr = D * Vis * PI_INVERSE * F;
	
	float3 kS = F;
	float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
	kD *= (1.0f - albedo.a);	
	
	// Diffuse BRDF (Lambertian)
	float3 Fd = LambertianDiffuse(albedo.rgb) * kD;
	
	// Calculate shadow
	float shadow = CalculateShadow(pin.ShadowPosH, ShadowMap, gsamShadow);
	
	float3 directLight = ((Fd + Fr) * (gSunLightStrength.rgb * gSunLightStrength.a * NdotL * (1.0f - shadow)));

	// Direct lighting value
	output.DirectLightingDepth = float4(directLight, (pin.PosH.z / pin.PosH.w));

	// Diffuse + Metallic G-Buffer value
	output.DiffuseMetallicGBuffer = albedo;
	
	// Normal + Roughness G-Buffer value
	output.NormalRoughnessGBuffer = pixelNormal;
	
	return output;
}