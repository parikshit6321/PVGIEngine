#include "RenderObject.h"

void RenderObject::Draw(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB, ID3D12Resource* matCB, 
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap, UINT cbvSrvDescriptorSize, UINT objCBByteSize, UINT matCBByteSize)
{
	if (isPostProcessingQuad)
	{
		DrawQuad(cmdList, srvDescriptorHeap);
	}
	else
	{
		cmdList->IASetVertexBuffers(0, 1, &(Geo->VertexBufferView()));
		cmdList->IASetIndexBuffer(&(Geo->IndexBufferView()));
		cmdList->IASetPrimitiveTopology(PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(Mat->DiffuseSrvHeapIndex, cbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
	}
}

void RenderObject::InitializeAsQuad(MeshGeometry* input, UINT quadIndex)
{
	SetWorldMatrix(&XMMatrixIdentity());
	SetObjCBIndex(0);
	Geo = input;
	SetPrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	SetIndexCount(Geo->DrawArgs[quadIndex].IndexCount);
	SetStartIndexLocation(Geo->DrawArgs[quadIndex].StartIndexLocation);
	SetBaseVertexLocation(Geo->DrawArgs[quadIndex].BaseVertexLocation);
	SetIsPostProcessingQuad(true);
}


int RenderObject::GetNumFramesDirty()
{
	return NumFramesDirty;
}

XMFLOAT4X4 * RenderObject::GetWorldMatrixPtr()
{
	return &World;
}

UINT RenderObject::GetObjCBIndex()
{
	return ObjCBIndex;
}

void RenderObject::SetObjCBIndex(UINT input)
{
	ObjCBIndex = input;
}

void RenderObject::SetMat(Material * input)
{
	Mat = input;
}

void RenderObject::SetGeo(MeshGeometry * input)
{
	Geo = input;
}

void RenderObject::SetPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY input)
{
	PrimitiveType = input;
}

void RenderObject::SetIndexCount(UINT input)
{
	IndexCount = input;
}

void RenderObject::SetStartIndexLocation(UINT input)
{
	StartIndexLocation = input;
}

void RenderObject::SetBaseVertexLocation(int input)
{
	BaseVertexLocation = input;
}

void RenderObject::SetWorldMatrix(XMMATRIX* input)
{
	XMStoreFloat4x4(&World, *input);
}

void RenderObject::SetIsPostProcessingQuad(bool input)
{
	isPostProcessingQuad = input;
}

void RenderObject::DecrementNumFramesDirty()
{
	--NumFramesDirty;
}

void RenderObject::DrawQuad(ID3D12GraphicsCommandList * cmdList, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap)
{
	cmdList->IASetVertexBuffers(0, 1, &(Geo->VertexBufferView()));
	cmdList->IASetIndexBuffer(&(Geo->IndexBufferView()));
	cmdList->IASetPrimitiveTopology(PrimitiveType);

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	
	cmdList->SetGraphicsRootDescriptorTable(0, tex);
	
	cmdList->DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}