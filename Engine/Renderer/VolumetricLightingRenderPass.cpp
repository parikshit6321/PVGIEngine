#include "VolumetricLightingRenderPass.h"

void VolumetricLightingRenderPass::Execute(ID3D12GraphicsCommandList *commandList, D3D12_CPU_DESCRIPTOR_HANDLE *depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	DISPATCH_COMPUTE(3, 1, (mClientWidth / 16), (mClientHeight / 16), 1)
}

void VolumetricLightingRenderPass::Draw(ID3D12GraphicsCommandList * commandList, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
}

void VolumetricLightingRenderPass::BuildRootSignature()
{
	SETUP_COMPUTE_ROOT_SIGNATURE(3, 1)
}

void VolumetricLightingRenderPass::BuildDescriptorHeaps()
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
	CREATE_SRV_UAV_HEAP(4)

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.Texture2D.MipLevels = mInputBuffers[0].Get()->GetDesc().MipLevels;

	md3dDevice->CreateShaderResourceView(mInputBuffers[0].Get(), &srvDesc, hDescriptor);
	
	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	// Create SRV to resource so we can sample the shadow map in a shader program.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescShadowMap = {};
	srvDescShadowMap.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescShadowMap.Format = DXGI_FORMAT_R32_FLOAT;
	srvDescShadowMap.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescShadowMap.Texture2D.MostDetailedMip = 0;
	srvDescShadowMap.Texture2D.MipLevels = 1;
	srvDescShadowMap.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDescShadowMap.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateShaderResourceView(mGBuffers[0].Get(), &srvDescShadowMap, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	md3dDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &srvDescShadowMap, hDescriptor);

	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	CREATE_UAV_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_UAV_DIMENSION_TEXTURE2D, 1)

	for (int i = 0; i < 1; ++i)
	{
		CREATE_UAV(cbvSrvUavDescriptorSize)
	}
}

void VolumetricLightingRenderPass::BuildPSOs()
{
	SETUP_COMPUTE_PSO()
}