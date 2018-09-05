#pragma once

#include "SceneManager.h"

using Microsoft::WRL::ComPtr;

class RenderPass
{
public:
	RenderPass() = default;
	void Initialize(ComPtr<ID3D12Device>, int, int);
	~RenderPass() = default;

protected:

	virtual void BuildRootSignature() = 0;
	virtual void BuildDescriptorHeaps() = 0;
	virtual void BuildShadersAndInputLayout() = 0;
	virtual void BuildPSOs() = 0;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> GetStaticSamplers();

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	
	ComPtr<ID3D12DescriptorHeap> mRtvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	ComPtr<ID3D12Device> md3dDevice;

	int mClientWidth = 1280;
	int mClientHeight = 720;
};

