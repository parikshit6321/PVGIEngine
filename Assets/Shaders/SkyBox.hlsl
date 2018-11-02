Texture2D    MainTex				 : register(t0);
Texture2D  	 DepthMap				 : register(t1);
TextureCube  SkyBoxTex				 : register(t2);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);

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
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vin.TexC.x - 1.0f, 1.0f - 2.0f * vin.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	ph.xyz /= ph.w;
	
	vout.PosV = mul(float4(ph.xyz, 1.0f), gSkyBoxMatrix).xyz;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 pixelColor = MainTex.Load(int3(pin.PosH.xy, 0));
	float depth = DepthMap.Load(int3(pin.PosH.xy, 0)).r;

	float4 skyBoxColor = pow(SkyBoxTex.Sample(gsamLinearWrap, pin.PosV), 2.2f);
	
	float4 result = lerp(pixelColor, skyBoxColor, floor(depth));

	return result;
}