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
	float3 directLight = MainTex.Sample(gsamLinearWrap, pin.TexC).rgb;
	
	float4 position = PositionDepthGBuffer.Sample(gsamLinearWrap, pin.TexC);
	float depth = position.a;
	
	if (depth == 1.0f)
		return float4(directLight, 1.0f);
	
	float4 albedo = DiffuseMetallicGBuffer.Sample(gsamLinearWrap, pin.TexC);
	float metallic = albedo.a;
	
	if (metallic > 0.9f)
		return float4(directLight, 1.0f);
	
	float4 normal = NormalRoughnessGBuffer.Sample(gsamLinearWrap, pin.TexC);
	
	float3 indirectDiffuseLight = albedo.rgb * SampleSHIndirectLighting(position.xyz, normal.xyz);
	
	float3 finalResult = directLight + indirectDiffuseLight;
	
	return float4(finalResult, 1.0f);
}