#pragma once

#include "ShadowMapRenderPass.h"
#include "DirectLightingRenderPass.h"
#include "VoxelInjectionRenderPass.h"
#include "SHIndirectRenderPass.h"
#include "IndirectLightingRenderPass.h"
#include "SkyBoxRenderPass.h"
#include "FXAARenderPass.h"
#include "ToneMappingRenderPass.h"
#include "ColorGradingRenderPass.h"

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	static void Initialize(ComPtr<ID3D12Device>, int, int, DXGI_FORMAT, DXGI_FORMAT);
	static void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*, FrameResource*);
	static void CopyToBackBuffer(ID3D12GraphicsCommandList*, ID3D12Resource*);

	static ShadowMapRenderPass shadowMapRenderPass;
	static DirectLightingRenderPass directLightingRenderPass;
	static VoxelInjectionRenderPass voxelInjectionRenderPass;
	static SHIndirectRenderPass shIndirectRenderPass;
	static IndirectLightingRenderPass indirectLightingRenderPass;
	static SkyBoxRenderPass skyBoxRenderPass;
	static FXAARenderPass fxaaRenderPass;
	static ToneMappingRenderPass toneMappingRenderPass;
	static ColorGradingRenderPass colorGradingRenderPass;
};