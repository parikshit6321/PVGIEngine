# PVGIEngine

Graphics Engine built in C++ and DirectX 12

- Lambertian diffuse and GGX specular height correlated PBR lighting.
- Forward+ shading.
- Using Unity 2017 as level editor and asset importer.
- Uncharted 2 style tonemapping.
- Color grading support using 2D LUTs.
- PCF based soft shadows.
- Progressive voxelization using depth and luma factors.
- Single bounce diffuse GI using cone tracing for every cell in a spherical harmonic grid and sampling SH grid using per-pixel normal.
- Cubemap reflections.
- Anti-aliasing using FXAA.
