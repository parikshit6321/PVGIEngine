#pragma once

#include "RenderPass.h"

class DeferredShadingRenderPass : public RenderPass
{
public:
	DeferredShadingRenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*,
		ID3D12Resource*, ID3D12Resource*, ID3D12Resource*) override;
	~DeferredShadingRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildShaders() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;
};