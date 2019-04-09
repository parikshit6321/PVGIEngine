#pragma once

#include "RenderPass.h"

class VoxelInjectionRenderPass : public RenderPass
{
public:
	VoxelInjectionRenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*, FrameResource*) override;
	~VoxelInjectionRenderPass() = default;

	UINT voxelResolution = 64;
	float worldVolumeBoundary = 50.0f;
	
protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;
};