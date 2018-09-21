#define TONEMAP_FUNCTION(x) (((x*(A*x + C * B) + D * E) / (x*(A*x + B) + D * F)) - E / F)
#define A (0.15f)
#define B (0.50f)
#define C (0.10f)
#define D (0.20f)
#define E (0.02f)
#define F (0.30f)

inline float3 Uncharted2Tonemap(float3 x)
{
	return TONEMAP_FUNCTION(x);
}