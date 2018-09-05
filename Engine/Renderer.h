#pragma once

#include "GBufferRenderPass.h"
#include "DeferredShadingRenderPass.h"

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	static GBufferRenderPass gBufferRenderPass;
	static DeferredShadingRenderPass deferredShadingRenderPass;
};

