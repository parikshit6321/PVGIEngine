#pragma once

#include "../Utilities/PVGIDecl.h"

using Microsoft::WRL::ComPtr;

// Render Pass Steps :

// 1. Set the PSO
// 2. Set output buffers from Generic Read state to Render Target state
// 3. Clear the render target views
// 4. Set the render targets
// 5. Set the SRV heap
// 6. Set the root signature
// 7. Set the constant buffer view for PassCB
// 8. Draw scene/quad
// 9. Set output buffers from Render Target state to Generic Read state

class RenderPass
{
public:
	RenderPass() = default;
	void Initialize(ComPtr<ID3D12Device>, int, int, DXGI_FORMAT, DXGI_FORMAT, ComPtr<ID3D12Resource>*, 
		ComPtr<ID3D12Resource>*, ComPtr<ID3D12Resource>*, ComPtr<ID3D12Resource>, std::wstring, std::wstring, 
		bool isComputePass = false);
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*, FrameResource*) = 0;
	~RenderPass() = default;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	ComPtr<ID3D12DescriptorHeap> mRtvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mDsvDescriptorHeap = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12Resource>* mInputBuffers;
	ComPtr<ID3D12Resource>* mOutputBuffers;
	ComPtr<ID3D12Resource>* mGBuffers;
	ComPtr<ID3D12Resource>* mVoxelGrids;
	ComPtr<ID3D12Resource> mDepthStencilBuffer;

protected:

	virtual void BuildRootSignature() = 0;
	virtual void BuildDescriptorHeaps() = 0;
	virtual void BuildPSOs() = 0;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) = 0;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> GetStaticSamplers();

	ComPtr<ID3DBlob> mVertexShader;
	ComPtr<ID3DBlob> mPixelShader;
	ComPtr<ID3DBlob> mComputeShader;
	
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12Device> md3dDevice;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	int mClientWidth = 1280;
	int mClientHeight = 720;
};

