#pragma once

#include "GBufferRenderPass.h"
#include "DeferredShadingRenderPass.h"
#include "ToneMappingRenderPass.h"

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	static void Initialize(ComPtr<ID3D12Device>, int, int, DXGI_FORMAT, DXGI_FORMAT);
	static void CopyToBackBuffer(ID3D12GraphicsCommandList*, ID3D12Resource*);

	static GBufferRenderPass gBufferRenderPass;
	static DeferredShadingRenderPass deferredShadingRenderPass;
	static ToneMappingRenderPass toneMappingRenderPass;
};

