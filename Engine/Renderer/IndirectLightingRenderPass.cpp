#include "IndirectLightingRenderPass.h"

void IndirectLightingRenderPass::Execute(ID3D12GraphicsCommandList *commandList, D3D12_CPU_DESCRIPTOR_HANDLE *depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	DISPATCH_COMPUTE(8, 1, (mClientWidth / 16), (mClientHeight / 16), 1)
}

void IndirectLightingRenderPass::Draw(ID3D12GraphicsCommandList * commandList, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
}

void IndirectLightingRenderPass::BuildRootSignature()
{
	SETUP_COMPUTE_ROOT_SIGNATURE(8, 1)
}

void IndirectLightingRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[1];

	CREATE_OUTPUT_BUFFER_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, mClientWidth, mClientHeight, 1)

	for (int i = 0; i < 1; ++i)
	{
		CREATE_OUTPUT_BUFFER_RESOURCE(nullptr)
	}

	//
	// Create the SRV and UAV heap.
	//
	CREATE_SRV_UAV_HEAP(9)

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = mBackBufferFormat;
	srvDesc.Texture2D.MipLevels = 1;

	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	md3dDevice->CreateShaderResourceView(mGBuffers[1].Get(), &srvDesc, hDescriptor);

	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);
	
	md3dDevice->CreateShaderResourceView(mGBuffers[2].Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	srvDesc.Texture2D.MipLevels = mInputBuffers[0].Get()->GetDesc().MipLevels;

	md3dDevice->CreateShaderResourceView(mInputBuffers[0].Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	// Create SRV to depth map so we can compute world-space position
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescDepthMap = {};
	srvDescDepthMap.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescDepthMap.Format = DXGI_FORMAT_R32_FLOAT;
	srvDescDepthMap.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescDepthMap.Texture2D.MostDetailedMip = 0;
	srvDescDepthMap.Texture2D.MipLevels = 1;
	srvDescDepthMap.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDescDepthMap.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &srvDescDepthMap, hDescriptor);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescVoxelGrid = {};
	srvDescVoxelGrid.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescVoxelGrid.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDescVoxelGrid.Texture3D.MostDetailedMip = 0;
	srvDescVoxelGrid.Texture3D.ResourceMinLODClamp = 0.0f;
	srvDescVoxelGrid.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDescVoxelGrid.Texture3D.MipLevels = 1;

	for (int i = 0; i < 3; ++i)
	{
		hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

		md3dDevice->CreateShaderResourceView(mVoxelGrids[i].Get(), &srvDescVoxelGrid, hDescriptor);
	}

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescCubeMap = {};
	srvDescCubeMap.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescCubeMap.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDescCubeMap.TextureCube.MostDetailedMip = 0;
	srvDescCubeMap.TextureCube.MipLevels = SceneManager::GetScenePtr()->mTextures[2 * SceneManager::GetScenePtr()->numberOfUniqueObjects]->Resource->GetDesc().MipLevels;
	srvDescCubeMap.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDescCubeMap.Format = SceneManager::GetScenePtr()->mTextures[2 * SceneManager::GetScenePtr()->numberOfUniqueObjects]->Resource->GetDesc().Format;

	md3dDevice->CreateShaderResourceView(SceneManager::GetScenePtr()->mTextures[2 * SceneManager::GetScenePtr()->numberOfUniqueObjects]->Resource.Get(),
		&srvDescCubeMap, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	CREATE_UAV_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_UAV_DIMENSION_TEXTURE2D, 1)

	for (int i = 0; i < 1; ++i)
	{
		CREATE_UAV(cbvSrvUavDescriptorSize)
	}
}

void IndirectLightingRenderPass::BuildPSOs()
{
	SETUP_COMPUTE_PSO()
}