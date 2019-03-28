inline float3 PackNormal(float3 inputNormal)
{
	return (inputNormal * 0.5f + 0.5f);
}

inline float3 UnpackNormal(float3 inputNormal)
{
	return (inputNormal * 2.0f - 1.0f);
}