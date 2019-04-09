#include "SkyBoxRenderPass.h"

void SkyBoxRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr, 
	FrameResource* mCurrFrameResource)
{
	DISPATCH_COMPUTE(2, 1, (mClientWidth / 16), (mClientHeight / 16), 1)
}

void SkyBoxRenderPass::Draw(ID3D12GraphicsCommandList * commandList, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
}

void SkyBoxRenderPass::BuildRootSignature()
{
	SETUP_COMPUTE_ROOT_SIGNATURE(2, 1)
}

void SkyBoxRenderPass::BuildDescriptorHeaps()
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
	CREATE_SRV_UAV_HEAP(3)

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

	// Add the input texture to the srv heap
	md3dDevice->CreateShaderResourceView(mInputBuffers[0].Get(), &srvDesc, hDescriptor);

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

void SkyBoxRenderPass::BuildPSOs()
{
	SETUP_COMPUTE_PSO()
}