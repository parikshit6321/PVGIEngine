#include "Renderer.h"

ShadowMapRenderPass Renderer::shadowMapRenderPass;
GBufferRenderPass Renderer::gBufferRenderPass;
DeferredShadingRenderPass Renderer::deferredShadingRenderPass;
VoxelInjectionRenderPass Renderer::voxelInjectionRenderPass;
SHIndirectRenderPass Renderer::shIndirectRenderPass;
IndirectLightingRenderPass Renderer::indirectLightingRenderPass;
SkyBoxRenderPass Renderer::skyBoxRenderPass;
FXAARenderPass Renderer::fxaaRenderPass;
ToneMappingRenderPass Renderer::toneMappingRenderPass;
ColorGradingRenderPass Renderer::colorGradingRenderPass;

void Renderer::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight,
	DXGI_FORMAT inputFormatBackBuffer, DXGI_FORMAT inputFormatDepthBuffer)
{
	shadowMapRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, nullptr, nullptr, L"ShadowMap.hlsl", L"");
	
	gBufferRenderPass.Initialize(inputDevice, inputWidth, inputHeight, 
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, nullptr, nullptr, L"GBufferWrite.hlsl", L"");
	
	deferredShadingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, shadowMapRenderPass.mOutputBuffers, 
		gBufferRenderPass.mOutputBuffers, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"DeferredShading.hlsl", L"");
	
	voxelInjectionRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, deferredShadingRenderPass.mOutputBuffers, 
		gBufferRenderPass.mOutputBuffers, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"", L"VoxelInjection.hlsl", true);
	
	shIndirectRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, 
		voxelInjectionRenderPass.mOutputBuffers, gBufferRenderPass.mDepthStencilBuffer, L"", L"SHIndirectConeTracing.hlsl", true);

	indirectLightingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, deferredShadingRenderPass.mOutputBuffers, 
		gBufferRenderPass.mOutputBuffers, shIndirectRenderPass.mOutputBuffers, gBufferRenderPass.mDepthStencilBuffer, 
		L"IndirectLighting.hlsl", L"");
	
	skyBoxRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, indirectLightingRenderPass.mOutputBuffers, 
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"SkyBox.hlsl", L"");

	toneMappingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, skyBoxRenderPass.mOutputBuffers,
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"ToneMapping.hlsl", L"");

	fxaaRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, toneMappingRenderPass.mOutputBuffers,
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"FXAA.hlsl", L"");
	
	colorGradingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, fxaaRenderPass.mOutputBuffers, 
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"ColorGrading.hlsl", L"");
}

void Renderer::CopyToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource * backBuffer)
{
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(colorGradingRenderPass.mOutputBuffers[0].Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

	commandList->CopyResource(backBuffer, Renderer::colorGradingRenderPass.mOutputBuffers[0].Get());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(colorGradingRenderPass.mOutputBuffers[0].Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ));
}
