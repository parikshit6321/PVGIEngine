#pragma once
#include "RenderPass.h"
class ToneMappingRenderPass :
	public RenderPass
{
public:
	ToneMappingRenderPass() = default;
	~ToneMappingRenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildShaders() override;
	virtual void BuildPSOs() override;
};