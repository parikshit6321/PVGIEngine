#define EDGE_THRESHOLD_MIN 0.0312f
#define EDGE_THRESHOLD_MAX 0.125f
#define SUBPIXEL_QUALITY 0.75f
#define ITERATIONS 12

static const float QUALITY[12] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 
							2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f };

// Get luma value of the color
float GetColorLuma(float3 inputColor){
    return sqrt(dot(inputColor, float3(0.299f, 0.587f, 0.114f)));
}