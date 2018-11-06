# PVGIEngine

Graphics Engine built in C++ and DirectX 12

- Lambertian diffuse and GGX specular height correlated PBR lighting.
- Deferred shading.
- Using Unity 2017 as level editor and asset importer.
- Uncharted 2 style tonemapping.
- Quad based sky.
- Color grading support using 2D LUTs.
- PCF based soft shadows.
- Progressive voxelization using the compute shader (0.24 ms).
- Two bounces of diffuse GI using cone tracing for every cell in a spherical harmonic grid and sampling SH grid using per-pixel normal (0.35 ms).
- Anti-aliasing using FXAA.
