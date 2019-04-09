#include "ToneMappingRenderPass.h"

void ToneMappingRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	DISPATCH_COMPUTE(1, 1, (mClientWidth / 16), (mClientHeight / 16), 1)
}

void ToneMappingRenderPass::Draw(ID3D12GraphicsCommandList * commandList, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
}

void ToneMappingRenderPass::BuildRootSignature()
{
	SETUP_COMPUTE_ROOT_SIGNATURE(1, 1)
}

void ToneMappingRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[1];

	CREATE_OUTPUT_BUFFER_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, mBackBufferFormat,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, mClientWidth, mClientHeight, 1)

	for (int i = 0; i < 1; ++i)
	{
		CREATE_OUTPUT_BUFFER_RESOURCE(nullptr)
	}

	//
	// Create the SRV and UAV heap.
	//
	CREATE_SRV_UAV_HEAP(2)

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.Texture2D.MipLevels = 1;

	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	md3dDevice->CreateShaderResourceView(mInputBuffers[0].Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	CREATE_UAV_DESC(mBackBufferFormat, D3D12_UAV_DIMENSION_TEXTURE2D, 1)

	for (int i = 0; i < 1; ++i)
	{
		CREATE_UAV(cbvSrvUavDescriptorSize)
	}
}

void ToneMappingRenderPass::BuildPSOs()
{
	SETUP_COMPUTE_PSO()
}