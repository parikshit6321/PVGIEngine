#pragma once

#include "ShadowMapRenderPass.h"
#include "GBufferRenderPass.h"
#include "DeferredShadingRenderPass.h"
#include "VoxelInjectionRenderPass.h"
#include "SkyBoxRenderPass.h"
#include "ToneMappingRenderPass.h"
#include "ColorGradingRenderPass.h"

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	static void Initialize(ComPtr<ID3D12Device>, int, int, DXGI_FORMAT, DXGI_FORMAT);
	static void CopyToBackBuffer(ID3D12GraphicsCommandList*, ID3D12Resource*);

	static ShadowMapRenderPass shadowMapRenderPass;
	static GBufferRenderPass gBufferRenderPass;
	static DeferredShadingRenderPass deferredShadingRenderPass;
	static VoxelInjectionRenderPass voxelInjectionRenderPass;
	static SkyBoxRenderPass skyBoxRenderPass;
	static ToneMappingRenderPass toneMappingRenderPass;
	static ColorGradingRenderPass colorGradingRenderPass;
};

