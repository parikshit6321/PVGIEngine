#include "Renderer.h"

GBufferRenderPass Renderer::gBufferRenderPass;
DeferredShadingRenderPass Renderer::deferredShadingRenderPass;
ToneMappingRenderPass Renderer::toneMappingRenderPass;

void Renderer::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight,
	DXGI_FORMAT inputFormatBackBuffer, DXGI_FORMAT inputFormatDepthBuffer)
{
	gBufferRenderPass.Initialize(inputDevice, inputWidth, inputHeight, 
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr);
	deferredShadingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, gBufferRenderPass.mOutputBuffers);
	toneMappingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, deferredShadingRenderPass.mOutputBuffers);
}