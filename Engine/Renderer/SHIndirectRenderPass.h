#pragma once

#include "RenderPass.h"

class SHIndirectRenderPass : public RenderPass
{
public:
	SHIndirectRenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*,
		ID3D12Resource*, ID3D12Resource*, ID3D12Resource*) override;
	~SHIndirectRenderPass() = default;

	UINT gridResolution = 8;
	float worldVolumeBoundary = 50.0f;
	UINT voxelResolution = 128;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;
};