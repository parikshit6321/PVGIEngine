#pragma once

#include "GBufferRenderPass.h"

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	static GBufferRenderPass gBufferRenderPass;
};

