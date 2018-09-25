# PVGIEngine
PBR Lighting Engine built in C++ and DirectX 12
- Disney diffuse and GGX specular PBR lighting.
- Deferred shading.
- Using Unity 2017 as level editor and asset importer.
- Uncharted 2 style tonemapping.
- Quad based sky.
- Color grading support using 2D LUTs.
- PCF based soft shadows.
- Progressive voxelization using the compute shader (0.14 ms for 1280x720 resolution RSM).
- Single bounce diffuse GI using cone tracing and specular GI using ray tracing (6.6 ms for 1280x720 render texture on Nvidia GTX 1060 Max-Q).
