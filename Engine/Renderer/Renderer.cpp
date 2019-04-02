#include "Renderer.h"

ShadowMapRenderPass Renderer::shadowMapRenderPass;
DirectLightingRenderPass Renderer::directLightingRenderPass;
VoxelInjectionRenderPass Renderer::voxelInjectionRenderPass;
SHIndirectRenderPass Renderer::shIndirectRenderPass;
IndirectLightingRenderPass Renderer::indirectLightingRenderPass;
SkyBoxRenderPass Renderer::skyBoxRenderPass;
VolumetricLightingRenderPass Renderer::volumetricLightingRenderPass;
FXAARenderPass Renderer::fxaaRenderPass;
ToneMappingRenderPass Renderer::toneMappingRenderPass;
ColorGradingRenderPass Renderer::colorGradingRenderPass;

bool Renderer::bPerformConeTracing = true;
bool Renderer::bPerformShadowMapping = true;

void Renderer::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight,
	DXGI_FORMAT inputFormatBackBuffer, DXGI_FORMAT inputFormatDepthBuffer)
{
	shadowMapRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, nullptr, nullptr, L"ShadowMap.hlsl", L"");
	
	directLightingRenderPass.Initialize(inputDevice, inputWidth, inputHeight, 
		inputFormatBackBuffer, inputFormatDepthBuffer, shadowMapRenderPass.mOutputBuffers, 
		nullptr, nullptr, nullptr, L"DirectLighting.hlsl", L"");
	
	voxelInjectionRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, directLightingRenderPass.mOutputBuffers, 
		nullptr, nullptr, directLightingRenderPass.mDepthStencilBuffer, L"", L"VoxelInjection.hlsl", true);
	
	shIndirectRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, 
		voxelInjectionRenderPass.mOutputBuffers, nullptr, L"", L"SHIndirectConeTracing.hlsl", true);

	indirectLightingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, directLightingRenderPass.mOutputBuffers,
		directLightingRenderPass.mOutputBuffers, shIndirectRenderPass.mOutputBuffers, directLightingRenderPass.mDepthStencilBuffer, 
		L"", L"IndirectLighting.hlsl", true);

	skyBoxRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, indirectLightingRenderPass.mOutputBuffers,
		nullptr, nullptr, nullptr, L"", L"SkyBox.hlsl", true);

	volumetricLightingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, skyBoxRenderPass.mOutputBuffers,
		shadowMapRenderPass.mOutputBuffers, nullptr, nullptr,
		L"", L"VolumetricLighting.hlsl", true);

	toneMappingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, skyBoxRenderPass.mOutputBuffers,
		nullptr, nullptr, nullptr, L"", L"ToneMapping.hlsl", true);

	fxaaRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, toneMappingRenderPass.mOutputBuffers,
		nullptr, nullptr, nullptr, L"", L"FXAA.hlsl", true);
	
	colorGradingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, fxaaRenderPass.mOutputBuffers, 
		nullptr, nullptr, nullptr, L"", L"ColorGrading.hlsl", true);
}

void Renderer::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	FrameResource* mCurrFrameResource)
{
	bPerformConeTracing = !bPerformConeTracing;

	// Render the gBuffers
	directLightingRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	if (!bPerformConeTracing)
		// Inject lighting data into the voxel grids
		voxelInjectionRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);
	else
		// Cone trace indirect lighting and inject it into the spherical harmonic grids
		shIndirectRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	// Sample SH grid to compute indirect diffuse lighting
	indirectLightingRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	// Render skybox on the background pixels using a quad
	skyBoxRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	// Perform ray marching to compute volumetric lighting
	//volumetricLightingRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	// Perform anti-aliasing using FXAA
	fxaaRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	// Bring the texture down to LDR range from HDR using Uncharted 2 style tonemapping
	toneMappingRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);

	// Use 2D LUTs for color grading
	colorGradingRenderPass.Execute(commandList, depthStencilViewPtr, mCurrFrameResource);
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
