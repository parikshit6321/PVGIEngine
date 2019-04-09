#pragma once

#include "../SceneManagement/SceneManager.h"

#define SETUP_COMPUTE_PSO() D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};			\
																							\
computePSODesc.pRootSignature = mRootSignature.Get();										\
computePSODesc.CS =																			\
{																							\
	reinterpret_cast<BYTE*>(mComputeShader->GetBufferPointer()),							\
	mComputeShader->GetBufferSize()															\
};																							\
computePSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;										\
ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(&mPSO)));


#define SETUP_COMPUTE_ROOT_SIGNATURE(NUM_OF_SRV, NUM_OF_UAV) CD3DX12_DESCRIPTOR_RANGE srvTable0;	\
srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_OF_SRV, 0);										\
																							\
CD3DX12_DESCRIPTOR_RANGE uavTable0;															\
uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, NUM_OF_UAV, 0);										\
																							\
/* Root parameter can be a table, root descriptor or root constants. */						\
CD3DX12_ROOT_PARAMETER slotRootParameter[3];												\
																							\
/* Perfomance TIP: Order from most frequent to least frequent. */							\
slotRootParameter[0].InitAsConstantBufferView(0);											\
slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);									\
slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);									\
																							\
auto staticSamplers = GetStaticSamplers();													\
																							\
/* A root signature is an array of root parameters. */										\
CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,								\
(UINT)staticSamplers.size(), staticSamplers.data(),											\
D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);								\
																							\
/* create a root signature with a single slot which points to a */							\
/*	descriptor range consisting of a single constant buffer */								\
ComPtr<ID3DBlob> serializedRootSig = nullptr;												\
ComPtr<ID3DBlob> errorBlob = nullptr;														\
HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,		\
	serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());							\
																							\
if (errorBlob != nullptr)																	\
{																							\
	::OutputDebugStringA((char*)errorBlob->GetBufferPointer());								\
}																							\
ThrowIfFailed(hr);																			\
																							\
ThrowIfFailed(md3dDevice->CreateRootSignature(												\
	0,																						\
	serializedRootSig->GetBufferPointer(),													\
	serializedRootSig->GetBufferSize(),														\
	IID_PPV_ARGS(mRootSignature.GetAddressOf())));


#define DISPATCH_COMPUTE(NUM_OF_SRV, NUM_OF_UAV, NUMTHREADS_X, NUMTHREADS_Y, NUMTHREADS_Z)							\
ID3D12Resource* passCB = mCurrFrameResource->PassCB->Resource();													\
																													\
UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);\
																													\
commandList->SetPipelineState(mPSO.Get());																			\
																													\
ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };												\
commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);										\
																													\
for (int i = 0; i < NUM_OF_UAV; ++i)																				\
{																													\
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[i].Get(),					\
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));									\
}																													\
																													\
commandList->SetComputeRootSignature(mRootSignature.Get());															\
																													\
commandList->SetComputeRootConstantBufferView(0, passCB->GetGPUVirtualAddress());									\
																													\
CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());						\
																													\
commandList->SetComputeRootDescriptorTable(1, tex);																	\
																													\
tex.Offset(NUM_OF_SRV, cbvSrvUavDescriptorSize);																	\
																													\
commandList->SetComputeRootDescriptorTable(2, tex);																	\
																													\
commandList->Dispatch(NUMTHREADS_X, NUMTHREADS_Y, NUMTHREADS_Z);													\
																													\
for (int i = 0; i < NUM_OF_UAV; ++i)																				\
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[i].Get(),					\
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));		


#define CREATE_OUTPUT_BUFFER_DESC(DIMENSION, FORMAT, RESOURCE_FLAG, WIDTH, HEIGHT, DEPTH)	\
																\
D3D12_RESOURCE_DESC texDesc;									\
ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));				\
texDesc.Dimension = DIMENSION;									\
texDesc.Alignment = 0;											\
texDesc.MipLevels = 1;											\
texDesc.Format = FORMAT;										\
texDesc.SampleDesc.Count = 1;									\
texDesc.SampleDesc.Quality = 0;									\
texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;					\
texDesc.Flags = RESOURCE_FLAG;									\
texDesc.Width = WIDTH;											\
texDesc.Height = HEIGHT;										\
texDesc.DepthOrArraySize = DEPTH;								
														

#define CREATE_OUTPUT_BUFFER_RESOURCE(CLEAR_VAL_PTR)			\
	ThrowIfFailed(md3dDevice->CreateCommittedResource(			\
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),		\
		D3D12_HEAP_FLAG_NONE,									\
		&texDesc,												\
		D3D12_RESOURCE_STATE_GENERIC_READ,						\
		CLEAR_VAL_PTR,											\
		IID_PPV_ARGS(&mOutputBuffers[i])));


#define CREATE_SRV_UAV_HEAP(SIZE)								\
																\
D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};					\
srvHeapDesc.NumDescriptors = SIZE;								\
srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;		\
srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;	\
ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));


#define CREATE_UAV_DESC(FORMAT, DIMENSION, DEPTH)							\
																			\
D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};								\
																			\
uavDesc.Format = FORMAT;													\
uavDesc.ViewDimension = DIMENSION;											\
uavDesc.Texture2D.MipSlice = 0;												\
uavDesc.Texture2D.PlaneSlice = 0;											\
uavDesc.Texture3D.MipSlice = 0;												\
uavDesc.Texture3D.FirstWSlice = 0;											\
uavDesc.Texture3D.WSize = DEPTH;											
																			

#define CREATE_UAV(DESCRIPTOR_SIZE)																	\
																									\
	md3dDevice->CreateUnorderedAccessView(mOutputBuffers[i].Get(), nullptr, &uavDesc, hDescriptor);	\
	hDescriptor.Offset(1, DESCRIPTOR_SIZE);									