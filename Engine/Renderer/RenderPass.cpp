#include "RenderPass.h"

void RenderPass::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight, 
	DXGI_FORMAT inputFormatBackBuffer, DXGI_FORMAT inputFormatDepthBuffer, ComPtr <ID3D12Resource>* inputBuffers)
{
	md3dDevice = inputDevice;
	mClientWidth = inputWidth;
	mClientHeight = inputHeight;
	mBackBufferFormat = inputFormatBackBuffer;
	mDepthStencilFormat = inputFormatDepthBuffer;
	mInputBuffers = inputBuffers;


	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShaders();
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
