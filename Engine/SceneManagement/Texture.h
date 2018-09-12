#pragma once

#include <string>
#include <d3d12.h>
#include <wrl.h>

class Texture
{
public:
	Texture() = default;
	~Texture() = default;

	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};