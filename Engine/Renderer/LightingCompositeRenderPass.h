#pragma once

#include "RenderPass.h"

class LightingCompositeRenderPass :	public RenderPass
{
public:
	LightingCompositeRenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*,
		ID3D12Resource*, ID3D12Resource*, ID3D12Resource*) override;
	~LightingCompositeRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;
};