Texture2D    inputTexture1  : register(t0);
Texture2D	 inputTexture2  : register(t1);

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
	float3 input1 = inputTexture1.Sample(gsamLinearWrap, pin.TexC).rgb;
	float3 input2 = inputTexture2.Sample(gsamLinearWrap, pin.TexC).rgb;
	return float4((input1 + input2), 1.0f);
}