#pragma once

#include "RenderPass.h"

class DeferredShadingRenderPass : public RenderPass
{
public:
	DeferredShadingRenderPass() = default;
	~DeferredShadingRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildShadersAndInputLayout() override;
	virtual void BuildPSOs() override;
};