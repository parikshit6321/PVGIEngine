#include "IndirectLightingRenderPass.h"

void IndirectLightingRenderPass::Execute(ID3D12GraphicsCommandList *commandList, D3D12_CPU_DESCRIPTOR_HANDLE *depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	auto passCB = mCurrFrameResource->PassCB->Resource();
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	commandList->SetPipelineState(mPSO.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[0].Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	commandList->SetComputeRootSignature(mRootSignature.Get());

	commandList->SetComputeRootConstantBufferView(0, passCB->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	commandList->SetComputeRootDescriptorTable(1, tex);

	tex.Offset(8, cbvSrvUavDescriptorSize);

	commandList->SetComputeRootDescriptorTable(2, tex);

	commandList->Dispatch(mClientWidth / 16, mClientHeight / 16, 1);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[0].Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void IndirectLightingRenderPass::Draw(ID3D12GraphicsCommandList * commandList, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
}

void IndirectLightingRenderPass::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void IndirectLightingRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[1];

	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = mClientWidth;
	resourceDesc.Height = mClientHeight;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(mOutputBuffers[0].GetAddressOf())));

	//
	// Create the SRV and UAV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 9;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

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
	srvDescVoxelGrid.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
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

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateUnorderedAccessView(mOutputBuffers[0].Get(), nullptr, &uavDesc, hDescriptor);
}

void IndirectLightingRenderPass::BuildPSOs()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};

	computePSODesc.pRootSignature = mRootSignature.Get();
	computePSODesc.CS =
	{
		reinterpret_cast<BYTE*>(mComputeShader->GetBufferPointer()),
		mComputeShader->GetBufferSize()
	};
	computePSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(&mPSO)));
}