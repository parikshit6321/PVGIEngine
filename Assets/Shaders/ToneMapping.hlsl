// Include structures and functions for lighting.
#include "ToneMappingUtil.hlsl"

Texture2D Input  			: register(t0);
RWTexture2D<float4> Output	: register(u0);

SamplerState gsamLinearWrap	: register(s0);

[numthreads(16, 16, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
	Output[id.xy] = pow(float4(Uncharted2Tonemap(Input[id.xy].rgb), 1.0f), GAMMA_FACTOR);
}