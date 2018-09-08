float3 ApplyLut2d(Texture2D UserLUT, SamplerState gsamLinearWrap, float3 uvw, float3 scaleOffset)
{
	// Strip format where `height = sqrt(width)`
	uvw.z *= scaleOffset.z;
	float shift = floor(uvw.z);
	uvw.xy = uvw.xy * scaleOffset.z * scaleOffset.xy + scaleOffset.xy * 0.5f;
	uvw.x += shift * scaleOffset.y;
	float4 color1 = UserLUT.Sample(gsamLinearWrap, uvw.xy);
	float4 color2 = UserLUT.Sample(gsamLinearWrap, (uvw.xy + float2(scaleOffset.y, 0.0f)));
	float3 color3 = lerp(color1.rgb, color2.rgb, (uvw.z - shift));
	return color3;
}

float3 GammaToLinearSpace(float3 input)
{
	// Approximate version from http://chilliant.blogspot.com.au/2012/08/srgb-approximations-for-hlsl.html?m=1
	return input * (input * (input * 0.305306011f + 0.682171111f) + 0.012522878f);
}

float3 LinearToGammaSpace(float3 input)
{
	input = max(input, float3(0.0f, 0.0f, 0.0f));
	// An almost-perfect approximation from http://chilliant.blogspot.com.au/2012/08/srgb-approximations-for-hlsl.html?m=1
	return max(1.055f * pow(input, 0.416666667f) - 0.055f, 0.f);
}