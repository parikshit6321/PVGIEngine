// Include structures and functions for lighting.
#include "LightingUtil.hlsl"
#include "CommonUtil.hlsl"

Texture2D	 Input				 	 : register(t0);
Texture2D	 ShadowMap			 	 : register(t1);
Texture2D	 DepthMap				 : register(t2);

RWTexture2D<float4> Output			 : register(u0);

SamplerState gsamLinearWrap			 : register(s0);
SamplerState gsamAnisotropicWrap	 : register(s1);
SamplerComparisonState gsamShadow	 : register(s2);

// Volumetric Lighting Constants
#define MAX_RAY_LENGTH 400.0f
#define SAMPLE_COUNT 64
#define SCATTERING_COEFFICIENT 0.02f
#define EXTINCTION_COEFFICIENT 0.005f
#define MIE_G_VALUE 0.6f
#define MIE_G_VECTOR float4((1.0f - (MIE_G_VALUE * MIE_G_VALUE)), (1.0f + (MIE_G_VALUE * MIE_G_VALUE)), (2.0f * MIE_G_VALUE), (1.0f / (4.0f * 3.14159f)))
#define SHADOW_EPSILON 0.003f

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

// Get the world position from linear depth
inline float3 GetWorldPosition(float depth, uint2 id)
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

inline float IsShadowed(float3 wpos)
{
	float4 shadowPosH = mul(float4(wpos, 1.0f), gShadowTransform);
	
	// Complete projection by doing division by w
	shadowPosH.xyz /= shadowPosH.w;

	float shadowed = CalculateShadow(shadowPosH, ShadowMap, gsamShadow);

	return shadowed;
}

inline float MieScattering(float cosAngle, float4 g)
{
	return (g.w * (g.x / (pow(max((g.y - g.z * cosAngle), 0.0f), 1.5f))));			
}


inline float4 RayMarch(float2 screenPos, float3 rayStart, float3 rayDir, float rayLength)
{
	//float2 interleavedPos = (fmod(floor(screenPos.xy), 8.0f));
	//float offset = ditherTexture.SampleLevel(gsamLinearWrap, ((interleavedPos / 8.0f) + float2(0.5f / 8.0f, 0.5f / 8.0f)), 0).r;
	
	float stepCount = SAMPLE_COUNT;

	float stepSize = rayLength / stepCount;
	float3 step = rayDir * stepSize;

	//float3 currentPosition = rayStart + step * offset;
	float3 currentPosition = rayStart + step;

	float4 vlight = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float extinction = 0.0f;
	float cosAngle = dot(gSunLightDirection.xyz, rayDir);

	for (float i = 0.0f; i < stepCount; i += 1.0f)
	{
		float shadowed = IsShadowed(currentPosition);
		float density = 1.0f;

		float scattering = SCATTERING_COEFFICIENT * stepSize * density;
		extinction += EXTINCTION_COEFFICIENT * stepSize * density;

		//float4 light = shadowed * scattering * exp(-extinction);
		float4 light = 1.0f - shadowed;

		vlight += light;

		currentPosition += step;				
	}
	
	vlight /= SAMPLE_COUNT;
	
	// apply phase function for dir light
	//vlight *= MieScattering(cosAngle, MIE_G_VECTOR);

	// apply light's color
	vlight.xyz *= gSunLightStrength.xyz;

	vlight.xyz = max(float3(0.0f, 0.0f, 0.0f), vlight.xyz);

	//vlight.w = exp(-extinction);

	return vlight;
}

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	float4 input = Input[id.xy];
	float3 position = GetWorldPosition(input.a, id.xy);
	
	float3 rayStart = gEyePosW;
	float3 rayDir = position - rayStart;				

	float rayLength = length(rayDir);
	rayDir /= rayLength;

	rayLength = min(rayLength, MAX_RAY_LENGTH);

	float2 screenPos = float2((id.x *gInvRenderTargetSize.x), (id.y * gInvRenderTargetSize.y));

	float4 volumetricLighting = RayMarch(screenPos, rayStart, rayDir, rayLength);
	
	//input.rgb *= volumetricLighting.w;

	Output[id.xy] = float4((input.rgb + volumetricLighting.rgb), input.a);
}