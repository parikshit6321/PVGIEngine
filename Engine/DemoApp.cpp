//***************************************************************************************
// DemoApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "Renderer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

class DemoApp : public D3DApp
{
public:
    DemoApp(HINSTANCE hInstance);
    DemoApp(const DemoApp& rhs) = delete;
    DemoApp& operator=(const DemoApp& rhs) = delete;
    ~DemoApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildPSOs();
    void BuildFrameResources();
    void DrawRenderObjects(ID3D12GraphicsCommandList* cmdList);
	void DrawPostProcessingQuad(ID3D12GraphicsCommandList* cmdList);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> GetStaticSamplers();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignaturePostProcessing = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvPostProcessingDescriptorHeap = nullptr;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPostProcessingPSO = nullptr;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        DemoApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

DemoApp::DemoApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

DemoApp::~DemoApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool DemoApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	SceneManager::LoadScene("../Assets/Scenes/DemoScene1.txt", md3dDevice, mCommandList);
	Renderer::gBufferRenderPass.Initialize(md3dDevice, mClientWidth, mClientHeight, mBackBufferFormat, mDepthStencilFormat, nullptr);
	BuildFrameResources();
	BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildPSOs();
	
    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void DemoApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.33f*MathHelper::Pi, AspectRatio(), 0.1f, 500.0f);
    XMStoreFloat4x4(&mProj, P);
}

void DemoApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % 3;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void DemoApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), Renderer::gBufferRenderPass.mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	for (int i = 0; i < 3; ++i)
	{
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Renderer::gBufferRenderPass.mOutputBuffers[i].Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
	}
	
    // Clear the back buffer and depth buffer.
	for (int i = 0; i < 3; ++i)
	{
		mCommandList->ClearRenderTargetView(CD3DX12_CPU_DESCRIPTOR_HANDLE(
			Renderer::gBufferRenderPass.mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			i,
			mRtvDescriptorSize), Colors::Black, 0, nullptr);
	}
	
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(3, &CD3DX12_CPU_DESCRIPTOR_HANDLE(
		Renderer::gBufferRenderPass.mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		0,
		mRtvDescriptorSize), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { Renderer::gBufferRenderPass.mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(Renderer::gBufferRenderPass.mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderObjects(mCommandList.Get());

	// Indicate a state transition on the resource usage.
	for (int i = 0; i < 3; ++i)
	{
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Renderer::gBufferRenderPass.mOutputBuffers[i].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	}

	ID3D12DescriptorHeap* descriptorHeapsPostProcessing[] = { mSrvPostProcessingDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeapsPostProcessing), descriptorHeapsPostProcessing);

	mCommandList->SetGraphicsRootSignature(mRootSignaturePostProcessing.Get());

	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetPipelineState(mPostProcessingPSO.Get());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);

	DrawPostProcessingQuad(mCommandList.Get());

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void DemoApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    SetCapture(mhMainWnd);
}

void DemoApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void DemoApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        
    }

}
 
void DemoApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
void DemoApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = SceneManager::GetScenePtr()->cameraPosition.x;
	mEyePos.y = SceneManager::GetScenePtr()->cameraPosition.y;
	mEyePos.z = SceneManager::GetScenePtr()->cameraPosition.z;

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void DemoApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto &e : SceneManager::GetScenePtr()->mOpaqueRObjects)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->GetNumFramesDirty() > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(e->GetWorldMatrixPtr());
			
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			
			currObjectCB->CopyData(e->GetObjCBIndex(), objConstants);

			// Next FrameResource need to be updated too.
			e->DecrementNumFramesDirty();
		}
	}
}

void DemoApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : SceneManager::GetScenePtr()->mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			MaterialConstants matConstants;
			matConstants.Metallic = mat->Metallic;
			
			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void DemoApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 0.1f;
	mMainPassCB.FarZ = 500.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.SunLightStrength = { SceneManager::GetScenePtr()->lightStrength.x, 
									 SceneManager::GetScenePtr()->lightStrength.y, 
									 SceneManager::GetScenePtr()->lightStrength.z, 1.0f };
	mMainPassCB.SunLightDirection = { SceneManager::GetScenePtr()->lightDirection.x, 
									  SceneManager::GetScenePtr()->lightDirection.y, 
									  SceneManager::GetScenePtr()->lightDirection.z, 1.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DemoApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTablePostProcessing;
	texTablePostProcessing.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameterPostProcessing[2];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameterPostProcessing[0].InitAsDescriptorTable(1, &texTablePostProcessing, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameterPostProcessing[1].InitAsConstantBufferView(0);

	auto staticSamplersPostProcessing = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDescPostProcessing(2, slotRootParameterPostProcessing,
		(UINT)staticSamplersPostProcessing.size(), staticSamplersPostProcessing.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSigPostProcessing = nullptr;
	ComPtr<ID3DBlob> errorBlobPostProcessing = nullptr;
	HRESULT hrPostProcessing = D3D12SerializeRootSignature(&rootSigDescPostProcessing, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSigPostProcessing.GetAddressOf(), errorBlobPostProcessing.GetAddressOf());

	if (errorBlobPostProcessing != nullptr)
	{
		::OutputDebugStringA((char*)errorBlobPostProcessing->GetBufferPointer());
	}
	ThrowIfFailed(hrPostProcessing);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSigPostProcessing->GetBufferPointer(),
		serializedRootSigPostProcessing->GetBufferSize(),
		IID_PPV_ARGS(mRootSignaturePostProcessing.GetAddressOf())));

}

void DemoApp::BuildDescriptorHeaps()
{
	//
	// Create the Post Processing SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvPostProcessingHeapDesc = {};
	srvPostProcessingHeapDesc.NumDescriptors = 3;
	srvPostProcessingHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvPostProcessingHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvPostProcessingHeapDesc, IID_PPV_ARGS(&mSrvPostProcessingDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hPostProcessingDescriptor(mSrvPostProcessingDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvPostProcessingDesc = {};
	srvPostProcessingDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvPostProcessingDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvPostProcessingDesc.Texture2D.MostDetailedMip = 0;
	srvPostProcessingDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvPostProcessingDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvPostProcessingDesc.Texture2D.MipLevels = 1;

	for (int i = 0; i < 3; ++i)
	{
		md3dDevice->CreateShaderResourceView(Renderer::gBufferRenderPass.mOutputBuffers[i].Get(), &srvPostProcessingDesc, hPostProcessingDescriptor);

		hPostProcessingDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
}

void DemoApp::BuildShadersAndInputLayout()
{
	mShaders["deferredShadingVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredShading.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["deferredShadingPS"] = d3dUtil::CompileShader(L"Shaders\\DeferredShading.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}

void DemoApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC postProcessingPsoDesc;

	//
	// PSO for post processing quad.
	//
	ZeroMemory(&postProcessingPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	postProcessingPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	postProcessingPsoDesc.pRootSignature = mRootSignaturePostProcessing.Get();
	postProcessingPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["deferredShadingVS"]->GetBufferPointer()),
		mShaders["deferredShadingVS"]->GetBufferSize()
	};
	postProcessingPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["deferredShadingPS"]->GetBufferPointer()),
		mShaders["deferredShadingPS"]->GetBufferSize()
	};
	postProcessingPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	postProcessingPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	postProcessingPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	postProcessingPsoDesc.SampleMask = UINT_MAX;
	postProcessingPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	postProcessingPsoDesc.NumRenderTargets = 1;
	postProcessingPsoDesc.RTVFormats[0] = mBackBufferFormat;
	postProcessingPsoDesc.SampleDesc.Count = 1;
	postProcessingPsoDesc.SampleDesc.Quality = 0;
	postProcessingPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&postProcessingPsoDesc, IID_PPV_ARGS(&mPostProcessingPSO)));
}

void DemoApp::BuildFrameResources()
{
    for(int i = 0; i < 3; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)SceneManager::GetScenePtr()->mOpaqueRObjects.size(), (UINT)SceneManager::GetScenePtr()->mMaterials.size()));
    }
}

void DemoApp::DrawRenderObjects(ID3D12GraphicsCommandList* cmdList)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    for(size_t i = 0; i < SceneManager::GetScenePtr()->mOpaqueRObjects.size(); ++i)
    {
		SceneManager::GetScenePtr()->mOpaqueRObjects[i]->Draw(cmdList, objectCB, matCB, Renderer::gBufferRenderPass.mSrvDescriptorHeap, mCbvSrvDescriptorSize, objCBByteSize, matCBByteSize);
    }
}

void DemoApp::DrawPostProcessingQuad(ID3D12GraphicsCommandList* cmdList)
{
	SceneManager::GetScenePtr()->mQuadrObject->Draw(cmdList, nullptr, nullptr, mSrvPostProcessingDescriptorHeap, 0, 0, 0);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> DemoApp::GetStaticSamplers()
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