#include "IndirectLightingUtil.hlsl"

Texture2D    DiffuseMetallicGBuffer  : register(t0);
Texture2D	 NormalRoughnessGBuffer  : register(t1);
Texture2D	 Input				 	 : register(t2);

RWTexture2D<float4> Output			 : register(u0);

// Get the world position from linear depth
float3 GetWorldPosition(float depth, uint2 id)
{
    float z = depth * 2.0f - 1.0f;

	float2 texCoord = float2(((float)id.x * gInvRenderTargetSize.x), ((float)id.y * gInvRenderTargetSize.y));

    float4 clipSpacePosition = float4(texCoord * 2.0f - 1.0f, z, 1.0f);
    float4 viewSpacePosition = mul(clipSpacePosition, gInvProj);

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    float4 worldSpacePosition = mul(viewSpacePosition, gInvView);

    return worldSpacePosition.xyz;
}

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	float4 lighting = Input[id.xy];
	float4 position = float4(GetWorldPosition(lighting.a, id.xy), lighting.a);
	float4 normal = NormalRoughnessGBuffer[id.xy];
	float4 albedo = DiffuseMetallicGBuffer[id.xy];
	
	float linearRoughness = (normal.w * normal.w);
	
	float3 V = normalize(gEyePosW - position.xyz);
	
	float3 reflectedDirection = normalize(-V - (2.0f * dot(-V, normal.xyz) * normal.xyz));
	
	float3 reflectedColor = pow(SkyBoxTex.SampleLevel(gsamLinearWrap, reflectedDirection, 0), 2.2f);
	
	float3 L = -reflectedDirection;
	float3 H = normalize(V + L);

	float NdotV = abs(dot(normal.xyz, V)) + 1e-5f; // avoid artifact
	float LdotH = saturate(dot(L ,H));
	float NdotH = saturate(dot(normal.xyz ,H));
	float NdotL = saturate(dot(normal.xyz ,L));

	// BRDF : GGX Specular

	float energyBias = lerp(0.0f, 0.5f, linearRoughness);
	float fd90 = energyBias + (2.0f * LdotH * LdotH * linearRoughness);
	float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, albedo.a);
	
	// Specular BRDF
	float3 F = FresnelSchlick(f0, fd90, LdotH);
	float Vis = SmithGGXCorrelated(NdotV, NdotL, linearRoughness);
	float D = D_GGX(NdotH, linearRoughness);
	float3 Fr = D * F * Vis * PI_INVERSE;
	
	float3 indirectSpecular = (Fr * reflectedColor * NdotL);
	
	float3 kS = F;
	float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
	kD *= (1.0f - albedo.a);
	
	float3 indirectDiffuse = CalculateDiffuseIndirectLighting(position, normal.xyz, albedo.rgb) * kD;
	
	lighting.rgb += (indirectDiffuse + indirectSpecular);
	
	Output[id.xy] = lighting;
}