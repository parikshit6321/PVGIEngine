#include "VoxelInjectionRenderPass.h"

void VoxelInjectionRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	ID3D12Resource * passCB, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
	commandList->SetPipelineState(mPSO.Get());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(voxelGrid1.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	commandList->SetComputeRootSignature(mRootSignature.Get());

	commandList->SetComputeRoot32BitConstants(0, 1, &voxelResolution, 0);
	commandList->SetComputeRoot32BitConstants(0, 1, &worldVolumeBoundary, 1);

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	commandList->SetGraphicsRootDescriptorTable(0, tex);

	commandList->Dispatch(mClientWidth, mClientHeight, 1);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(voxelGrid1.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));

}

void VoxelInjectionRenderPass::Draw(ID3D12GraphicsCommandList *, ID3D12Resource *, ID3D12Resource *)
{
}

void VoxelInjectionRenderPass::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstants(2, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		0, nullptr,
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

void VoxelInjectionRenderPass::BuildDescriptorHeaps()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	texDesc.Alignment = 0;
	texDesc.Width = voxelResolution;
	texDesc.Height = voxelResolution;
	texDesc.DepthOrArraySize = voxelResolution;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_CLEAR_VALUE clearVal;
	clearVal.Color[0] = 0.0f;
	clearVal.Color[1] = 0.0f;
	clearVal.Color[2] = 0.0f;
	clearVal.Color[3] = 0.0f;
	clearVal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&clearVal,
		IID_PPV_ARGS(&voxelGrid1)));

	//
	// Create the SRV and UAV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 3;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with texture descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mInputBuffers[0].Get()->GetDesc().MipLevels;
	
	// Lighting texture as an SRV
	md3dDevice->CreateShaderResourceView(mInputBuffers[0].Get(), &srvDesc, hDescriptor);

	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	srvDesc.Texture2D.MipLevels = mGBuffers[2].Get()->GetDesc().MipLevels;

	// Position and depth texture as an SRV
	md3dDevice->CreateShaderResourceView(mGBuffers[2].Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbvSrvUavDescriptorSize);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;

	// Voxel grid 3D texture as a UAV
	md3dDevice->CreateUnorderedAccessView(voxelGrid1.Get(), nullptr, &uavDesc, hDescriptor);
}

void VoxelInjectionRenderPass::BuildPSOs()
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