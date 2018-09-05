#pragma once

#include "RenderPass.h"

class GBufferRenderPass : public RenderPass
{
public:
	GBufferRenderPass() = default;
	~GBufferRenderPass() = default;

	ComPtr<ID3D12Resource> mOutputBuffers[3];

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildShadersAndInputLayout() override;
	virtual void BuildPSOs() override;
};