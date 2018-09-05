#include "RenderPass.h"

void RenderPass::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight, DXGI_FORMAT inputFormat)
{
	md3dDevice = inputDevice;
	mClientWidth = inputWidth;
	mClientHeight = inputHeight;
	mDepthStencilFormat = inputFormat;

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildPSOs();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> RenderPass::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		1, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	return { linearWrap, anisotropicWrap };
}
