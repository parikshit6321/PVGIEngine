#include "MeshGeometry.h"

D3D12_VERTEX_BUFFER_VIEW MeshGeometry::VertexBufferView()const
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = VertexByteStride;
	vbv.SizeInBytes = VertexBufferByteSize;

	return vbv;
}

D3D12_INDEX_BUFFER_VIEW MeshGeometry::IndexBufferView()const
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = IndexFormat;
	ibv.SizeInBytes = IndexBufferByteSize;

	return ibv;
}

// We can free this memory after we finish upload to the GPU.
void MeshGeometry::DisposeUploaders()
{
	VertexBufferUploader = nullptr;
	IndexBufferUploader = nullptr;
}