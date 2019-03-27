Texture2D    Input					 : register(t0);
TextureCube  SkyBoxTex				 : register(t1);

RWTexture2D<float4>	Output			 : register(u0);

SamplerState gsamLinearWrap			 : register(s0);

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

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	float4 inputColor = Input[id.xy];
	
	float2 texCoord = float2(((float)id.x * gInvRenderTargetSize.x), ((float)id.y * gInvRenderTargetSize.y));
	texCoord.x = ((texCoord.x * 2.0f) - 1.0f);
	texCoord.y = (1.0f - (texCoord.y * 2.0f));
	
	float4 PosH = float4(texCoord, 1.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(PosH, gInvProj);
	ph.xyz /= ph.w;
	
	float3 PosV = mul(ph, gSkyBoxMatrix).xyz;
	
	float4 skyBoxColor = pow(SkyBoxTex.SampleLevel(gsamLinearWrap, PosV, 0), 2.2f);
	
	Output[id.xy] = float4(lerp(inputColor.rgb, skyBoxColor.rgb, floor(inputColor.a)), inputColor.a);
}