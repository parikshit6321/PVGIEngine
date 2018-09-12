#pragma once

#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "DDSTextureLoader.h"
#include "DxException.h"

class d3dUtil
{
public:

    static bool IsKeyDown(int vkeyCode);

	static UINT CalcConstantBufferByteSize(UINT byteSize);

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
};