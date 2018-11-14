# PVGIEngine

Graphics Engine built in C++ and DirectX 12

- Lambertian diffuse and GGX specular height correlated PBR lighting.
- Deferred shading.
- Using Unity 2017 as level editor and asset importer.
- Uncharted 2 style tonemapping.
- Quad based sky.
- Color grading support using 2D LUTs.
- PCF based soft shadows.
- Progressive voxelization using depth and luma factors.
- Single bounce diffuse GI using cone tracing for every cell in a spherical harmonic grid and sampling SH grid using per-pixel normal.
- Single bounce specular GI using ray tracing through the voxel grids.
- Total GI takes about 0.95 ms on Nvidia GTX 1060 Max-Q.
- Anti-aliasing using FXAA.
