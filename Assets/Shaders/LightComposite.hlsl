Texture2D    directLightingTexture	: register(t0);
Texture2D    firstBounceTexture  	: register(t1);
Texture2D	 secondBounceTexture 	: register(t2);

SamplerState gsamLinearWrap			 : register(s0);

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
	float4 directLighting = directLightingTexture.Load(int3(pin.PosH.xy, 0));
	float4 firstBounce = firstBounceTexture.Load(int3(pin.PosH.xy, 0));
	float4 secondBounce = secondBounceTexture.Load(int3(pin.PosH.xy, 0));
	
	float4 finalLighting = directLighting + firstBounce + secondBounce;
	
	return float4(finalLighting.rgb, 1.0f);
}