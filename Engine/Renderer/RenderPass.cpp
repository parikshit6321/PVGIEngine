#include "RenderPass.h"

void RenderPass::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight, 
	DXGI_FORMAT inputFormatBackBuffer, DXGI_FORMAT inputFormatDepthBuffer, ComPtr <ID3D12Resource>* inputBuffers,
	ComPtr<ID3D12Resource>* gBuffers, ComPtr<ID3D12Resource>* voxelGrids, ComPtr<ID3D12Resource> inputDepthBuffer,
	const std::wstring shaderName)
{
	md3dDevice = inputDevice;
	mClientWidth = inputWidth;
	mClientHeight = inputHeight;
	mBackBufferFormat = inputFormatBackBuffer;
	mDepthStencilFormat = inputFormatDepthBuffer;
	mInputBuffers = inputBuffers;
	mGBuffers = gBuffers;
	mVoxelGrids = voxelGrids;
	mDepthStencilBuffer = inputDepthBuffer;

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	mVertexShader = d3dUtil::CompileShader(L"../Assets/Shaders/" + shaderName, nullptr, "VS", "vs_5_1");
	mPixelShader = d3dUtil::CompileShader(L"../Assets/Shaders/" + shaderName, nullptr, "PS", "ps_5_1");
	mComputeShader = d3dUtil::CompileShader(L"../Assets/Shaders/VoxelInjection.hlsl", nullptr, "CS", "cs_5_1");

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildPSOs();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> RenderPass::GetStaticSamplers()
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

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		2, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return { linearWrap, anisotropicWrap, shadow };
}
