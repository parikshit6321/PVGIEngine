#pragma once

#include <string>
#include <DirectXMath.h>

class Material
{
public:
	Material() = default;
	~Material() = default;

	// Unique material name for lookup.
	std::string Name;

	// Index into constant buffer corresponding to this material.
	int MatCBIndex = -1;

	// Index into SRV heap for diffuse texture.
	int DiffuseSrvHeapIndex = -1;

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = 3 so that each frame resource gets the update.
	int NumFramesDirty = 3;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4 Metallic = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
};

