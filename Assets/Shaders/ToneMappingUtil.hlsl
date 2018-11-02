#define TONEMAP_FUNCTION(x) (((x*(A*x + CxB) + DxE) / (x*(A*x + B) + DxF)) - EdivF)
#define A (0.15f)
#define B (0.50f)
#define C (0.10f)
#define D (0.20f)
#define E (0.02f)
#define F (0.30f)

#define CxB (0.05f)
#define DxE (0.004f)
#define DxF (0.06f)
#define EdivF (0.06666667f)

#define GAMMA_FACTOR 0.45454545f

#define LINEAR_WHITE_POINT_VALUE_INVERSE (1.2629515f)

inline float3 Uncharted2Tonemap(float3 x)
{
	return TONEMAP_FUNCTION(x) * LINEAR_WHITE_POINT_VALUE_INVERSE;
}