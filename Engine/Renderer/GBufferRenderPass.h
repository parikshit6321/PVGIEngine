#pragma once

#include "RenderPass.h"

class GBufferRenderPass : public RenderPass
{
public:
	GBufferRenderPass() = default;
	~GBufferRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildShaders() override;
	virtual void BuildPSOs() override;
};