#include "VoxelInjectionRenderPass.h"

void VoxelInjectionRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	DISPATCH_COMPUTE(2, 5, (mClientWidth / 16), (mClientHeight / 16), 1)
}

void VoxelInjectionRenderPass::Draw(ID3D12GraphicsCommandList *, ID3D12Resource *, ID3D12Resource *)
{
}

void VoxelInjectionRenderPass::BuildRootSignature()
{
	SETUP_COMPUTE_ROOT_SIGNATURE(2, 5)
}

void VoxelInjectionRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[5];

	CREATE_OUTPUT_BUFFER_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, voxelResolution, voxelResolution, voxelResolution)

	for (int i = 0; i < 5; ++i)
	{
		texDesc.Width = voxelResolution / ((int)(pow(2, i)));
		texDesc.Height = voxelResolution / ((int)(pow(2, i)));
		texDesc.DepthOrArraySize = voxelResolution / ((int)(pow(2, i)));

		CREATE_OUTPUT_BUFFER_RESOURCE(nullptr)
	}

	//
	// Create the SRV and UAV heap.
	//
	CREATE_SRV_UAV_HEAP(7)

	//
	// Fill out the heap with texture descriptors.
	//
	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mInputBuffers[0].Get()->GetDesc().MipLevels;
	
	// Lighting texture as an SRV
	md3dDevice->CreateShaderResourceView(mInputBuffers[0].Get(), &srvDesc, hDescriptor);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescDepthMap = {};
	srvDescDepthMap.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescDepthMap.Format = DXGI_FORMAT_R32_FLOAT;
	srvDescDepthMap.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescDepthMap.Texture2D.MostDetailedMip = 0;
	srvDescDepthMap.Texture2D.MipLevels = 1;
	srvDescDepthMap.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDescDepthMap.Texture2D.PlaneSlice = 0;

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	md3dDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &srvDescDepthMap, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	CREATE_UAV_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_UAV_DIMENSION_TEXTURE3D, voxelResolution)

	for (int i = 0; i < 5; ++i)
	{
		uavDesc.Texture3D.WSize = voxelResolution / ((int)pow(2, i));

		CREATE_UAV(cbvSrvUavDescriptorSize)
	}
}

void VoxelInjectionRenderPass::BuildPSOs()
{
	SETUP_COMPUTE_PSO()
}