#pragma once

#include "RenderPass.h"

#define SHADOW_MAP_RESOLUTION 2048

class ShadowMapRenderPass :
	public RenderPass
{
public:
	ShadowMapRenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*, FrameResource*) override;
	~ShadowMapRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;

	D3D12_VIEWPORT mViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_RESOLUTION, (float)SHADOW_MAP_RESOLUTION, 0.0f, 1.0f };
	D3D12_RECT mScissorRect = { 0, 0, (int)SHADOW_MAP_RESOLUTION, (int)SHADOW_MAP_RESOLUTION };
};