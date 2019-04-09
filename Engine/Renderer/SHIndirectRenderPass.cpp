#include "SHIndirectRenderPass.h"

void SHIndirectRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	DISPATCH_COMPUTE(5, 3, (gridResolution / 4), (gridResolution / 4), (gridResolution / 4))
}

void SHIndirectRenderPass::Draw(ID3D12GraphicsCommandList *, ID3D12Resource *, ID3D12Resource *)
{
}

void SHIndirectRenderPass::BuildRootSignature()
{
	SETUP_COMPUTE_ROOT_SIGNATURE(5, 3)
}

void SHIndirectRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[3];

	CREATE_OUTPUT_BUFFER_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, gridResolution, gridResolution, gridResolution)

	for (int i = 0; i < 3; ++i)
		CREATE_OUTPUT_BUFFER_RESOURCE(nullptr)
	
	//
	// Create the SRV and UAV heap.
	//
	CREATE_SRV_UAV_HEAP(8)

	//
	// Fill out the heap with texture descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescVoxelGrid = {};
	srvDescVoxelGrid.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescVoxelGrid.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDescVoxelGrid.Texture3D.MostDetailedMip = 0;
	srvDescVoxelGrid.Texture3D.ResourceMinLODClamp = 0.0f;
	srvDescVoxelGrid.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDescVoxelGrid.Texture3D.MipLevels = 1;

	for (int i = 0; i < 5; ++i)
	{
		md3dDevice->CreateShaderResourceView(mVoxelGrids[i].Get(), &srvDescVoxelGrid, hDescriptor);

		hDescriptor.Offset(1, cbvSrvUavDescriptorSize);
	}

	CREATE_UAV_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_UAV_DIMENSION_TEXTURE3D, gridResolution)

	for (int i = 0; i < 3; ++i)
	{
		CREATE_UAV(cbvSrvUavDescriptorSize)
	}
}

void SHIndirectRenderPass::BuildPSOs()
{
	SETUP_COMPUTE_PSO()
}