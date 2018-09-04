#include "RenderObject.h"

void RenderObject::Draw(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB, ID3D12Resource* matCB, 
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap, UINT cbvSrvDescriptorSize, UINT objCBByteSize, UINT matCBByteSize)
{
	cmdList->IASetVertexBuffers(0, 1, &(Geo->VertexBufferView()));
	cmdList->IASetIndexBuffer(&(Geo->IndexBufferView()));
	cmdList->IASetPrimitiveTopology(PrimitiveType);

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex.Offset(Mat->DiffuseSrvHeapIndex, cbvSrvDescriptorSize);

	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ObjCBIndex*objCBByteSize;
	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + Mat->MatCBIndex*matCBByteSize;

	cmdList->SetGraphicsRootDescriptorTable(0, tex);
	cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
	cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

	cmdList->DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}