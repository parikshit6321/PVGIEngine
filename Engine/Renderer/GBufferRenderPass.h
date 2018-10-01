#pragma once

#include "RenderPass.h"

class GBufferRenderPass : public RenderPass
{
public:
	GBufferRenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*, 
		ID3D12Resource*, ID3D12Resource*, ID3D12Resource*) override;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView();
	~GBufferRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;
};