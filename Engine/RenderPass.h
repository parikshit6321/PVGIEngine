#pragma once

#include "SceneManager.h"

using Microsoft::WRL::ComPtr;

class RenderPass
{
public:
	RenderPass() = default;
	void Initialize(ComPtr<ID3D12Device>, int, int, DXGI_FORMAT, DXGI_FORMAT, ComPtr<ID3D12Resource>*);
	~RenderPass() = default;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	ComPtr<ID3D12DescriptorHeap> mRtvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12Resource>* mInputBuffers;
	ComPtr<ID3D12Resource> mOutputBuffers[3];

protected:

	virtual void BuildRootSignature() = 0;
	virtual void BuildDescriptorHeaps() = 0;
	virtual void BuildShadersAndInputLayout() = 0;
	virtual void BuildPSOs() = 0;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> GetStaticSamplers();

	ComPtr<ID3DBlob> mVertexShader;
	ComPtr<ID3DBlob> mPixelShader;
	
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12Device> md3dDevice;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	int mClientWidth = 1280;
	int mClientHeight = 720;
};

