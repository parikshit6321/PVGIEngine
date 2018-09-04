//***************************************************************************************
// DemoApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "../Common/MeshLoader.h"
#include "RenderObject.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

struct SceneObject
{
	SceneObject() = default;

	std::string			meshName;
	std::string			diffuseOpacityTextureName;
	std::string			normalRoughnessTextureName;
	XMFLOAT3			position;
	XMFLOAT4			rotation;
	XMFLOAT3			scale;
};

struct Scene
{
	Scene() = default;

	std::string			name;
	XMFLOAT3			cameraPosition;
	XMFLOAT3			lightDirection;
	XMFLOAT3			lightStrength;
	UINT				numberOfObjects;

	SceneObject*		objectsInScene;
};

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

	void LoadScene(std::string);
	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderObjects();
    void DrawRenderObjects(ID3D12GraphicsCommandList* cmdList);
	void DrawPostProcessingQuad(ID3D12GraphicsCommandList* cmdList);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> GetStaticSamplers();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignaturePostProcessing = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvPostProcessingDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, std::unique_ptr<SubmeshGeometry>> mSubMeshes;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
	ComPtr<ID3D12PipelineState> mPostProcessingPSO = nullptr;

	// Post processing quad render item
	std::unique_ptr<RenderObject> mQuadrObject;

	// Render items divided by PSO.
	std::vector<std::unique_ptr<RenderObject>> mOpaqueRObjects;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	Scene mScene;
	UINT mNumTexturesLoaded = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> mGBuffers[3];
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

	LoadScene("../Assets/Scenes/DemoScene2.txt");
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildMaterials();
    BuildRenderObjects();
    BuildFrameResources();
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
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mOpaquePSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	for (int i = 0; i < 3; ++i)
	{
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGBuffers[i].Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
	}
	
    // Clear the back buffer and depth buffer.
	for (int i = 0; i < 3; ++i)
	{
		mCommandList->ClearRenderTargetView(CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			(2 + i),
			mRtvDescriptorSize), Colors::Black, 0, nullptr);
	}
	
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(3, &CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		2,
		mRtvDescriptorSize), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderObjects(mCommandList.Get());

	// Indicate a state transition on the resource usage.
	for (int i = 0; i < 3; ++i)
	{
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGBuffers[i].Get(),
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
	mEyePos.x = mScene.cameraPosition.x;
	mEyePos.y = mScene.cameraPosition.y;
	mEyePos.z = mScene.cameraPosition.z;

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
	for(auto &e : mOpaqueRObjects)
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
	for(auto& e : mMaterials)
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
	mMainPassCB.SunLightStrength = { mScene.lightStrength.x, mScene.lightStrength.y, mScene.lightStrength.z, 1.0f };
	mMainPassCB.SunLightDirection = { mScene.lightDirection.x, mScene.lightDirection.y, mScene.lightDirection.z, 1.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DemoApp::LoadScene(std::string sceneFilePath)
{
	std::ifstream inputFile;
	inputFile.open(sceneFilePath, std::fstream::in);

	std::string name;
	XMFLOAT3 cameraPosition;
	XMFLOAT3 lightDirection;
	XMFLOAT3 lightStrength;
	UINT numberOfObjects;

	inputFile >> name;
	inputFile >> cameraPosition.x >> cameraPosition.y >> cameraPosition.z;
	inputFile >> lightDirection.x >> lightDirection.y >> lightDirection.z;
	inputFile >> lightStrength.x >> lightStrength.y >> lightStrength.z;
	inputFile >> numberOfObjects;

	mScene.name = name;
	mScene.cameraPosition = cameraPosition;
	mScene.lightDirection = lightDirection;
	mScene.lightStrength = lightStrength;
	mScene.numberOfObjects = numberOfObjects;

	mScene.objectsInScene = new SceneObject[numberOfObjects];

	for (int i = 0; i < numberOfObjects; ++i)
	{
		inputFile >> mScene.objectsInScene[i].meshName;
		inputFile >> mScene.objectsInScene[i].diffuseOpacityTextureName;
		inputFile >> mScene.objectsInScene[i].normalRoughnessTextureName;
		inputFile >> mScene.objectsInScene[i].position.x >> mScene.objectsInScene[i].position.y >> mScene.objectsInScene[i].position.z;
		inputFile >> mScene.objectsInScene[i].rotation.x >> mScene.objectsInScene[i].rotation.y >> mScene.objectsInScene[i].rotation.z >> mScene.objectsInScene[i].rotation.w;
		inputFile >> mScene.objectsInScene[i].scale.x >> mScene.objectsInScene[i].scale.y >> mScene.objectsInScene[i].scale.z;
	}

	inputFile.close();
}

void DemoApp::LoadTextures()
{
	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		if (mTextures.find(mScene.objectsInScene[i].diffuseOpacityTextureName) == mTextures.end())
		{
			std::wstringstream ws;
			ws << mScene.objectsInScene[i].diffuseOpacityTextureName.c_str();
			std::wstring wsName = ws.str();

			auto tex = std::make_unique<Texture>();
			tex->Name = mScene.objectsInScene[i].diffuseOpacityTextureName;
			tex->Filename = L"../Assets/Textures/" + wsName + L".dds";
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				mCommandList.Get(), tex->Filename.c_str(),
				tex->Resource, tex->UploadHeap));

			mTextures[tex->Name] = std::move(tex);

			++mNumTexturesLoaded;
		}

		if (mTextures.find(mScene.objectsInScene[i].normalRoughnessTextureName) == mTextures.end())
		{
			std::wstringstream ws;
			ws << mScene.objectsInScene[i].normalRoughnessTextureName.c_str();
			std::wstring wsName = ws.str();

			auto tex = std::make_unique<Texture>();
			tex->Name = mScene.objectsInScene[i].normalRoughnessTextureName;
			tex->Filename = L"../Assets/Textures/" + wsName + L".dds";
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				mCommandList.Get(), tex->Filename.c_str(),
				tex->Resource, tex->UploadHeap));

			mTextures[tex->Name] = std::move(tex);

			++mNumTexturesLoaded;
		}
	}
}

void DemoApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));

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
	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;

	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = mClientWidth;
	resourceDesc.Height = mClientHeight;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearVal;
	clearVal.Color[0] = 0.0f;
	clearVal.Color[1] = 0.0f;
	clearVal.Color[2] = 0.0f;
	clearVal.Color[3] = 1.0f;

	for (int i = 0; i < 3; i++) {
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		clearVal.Format = resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, 
			&resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearVal, IID_PPV_ARGS(mGBuffers[i].GetAddressOf())));
	}
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvhDescriptor(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

	rtvhDescriptor.Offset(2, mRtvDescriptorSize);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < 3; ++i)
	{
		md3dDevice->CreateRenderTargetView(mGBuffers[i].Get(), &rtvDesc, rtvhDescriptor);

		if (i != 2)
			rtvhDescriptor.Offset(1, mRtvDescriptorSize);
	}

	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mNumTexturesLoaded;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	auto it = mTextures.begin();

	while (it != mTextures.end())
	{
		srvDesc.Format = it->second->Resource->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = it->second->Resource->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(it->second->Resource.Get(), &srvDesc, hDescriptor);

		++it;

		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}

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
		md3dDevice->CreateShaderResourceView(mGBuffers[i].Get(), &srvPostProcessingDesc, hPostProcessingDescriptor);

		hPostProcessingDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
}

void DemoApp::BuildShadersAndInputLayout()
{
	mShaders["gBufferWriteVS"] = d3dUtil::CompileShader(L"Shaders\\GBufferWrite.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["gBufferWritePS"] = d3dUtil::CompileShader(L"Shaders\\GBufferWrite.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["deferredShadingVS"] = d3dUtil::CompileShader(L"Shaders\\DeferredShading.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["deferredShadingPS"] = d3dUtil::CompileShader(L"Shaders\\DeferredShading.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}

void DemoApp::BuildShapeGeometry()
{
    size_t totalVertexCount = 0;
	size_t currentStartIndexCount = 0;
	size_t currentBaseVertexLocation = 0;
	
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = mScene.name;

	UINT k = 0;

	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		if (mSubMeshes.find(mScene.objectsInScene[i].meshName) == mSubMeshes.end())
		{
			MeshLoader::MeshData tempMesh = MeshLoader::LoadModel(mScene.objectsInScene[i].meshName);

			totalVertexCount += tempMesh.Vertices.size();

			auto tempSubMesh = std::make_unique<SubmeshGeometry>();;
			tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
			tempSubMesh->StartIndexLocation = currentStartIndexCount;
			tempSubMesh->BaseVertexLocation = currentBaseVertexLocation;

			currentStartIndexCount += tempMesh.Indices32.size();
			currentBaseVertexLocation += tempMesh.Vertices.size();

			mSubMeshes[mScene.objectsInScene[i].meshName] = std::move(tempSubMesh);

			vertices.resize(totalVertexCount);

			for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
			{
				vertices[k].Pos = tempMesh.Vertices[i].Position;
				vertices[k].Normal = tempMesh.Vertices[i].Normal;
				vertices[k].TexC = tempMesh.Vertices[i].TexC;
				vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
			}

			indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

			geo->DrawArgs[mScene.objectsInScene[i].meshName] = *mSubMeshes[mScene.objectsInScene[i].meshName];
		}
	}

	// Create the post processing quad geometry
	MeshLoader::MeshData tempMesh = MeshLoader::CreateQuad(0.0f, 0.0f, 2.0f, 2.0f, 0.0f);

	totalVertexCount += tempMesh.Vertices.size();

	auto tempSubMesh = std::make_unique<SubmeshGeometry>();;
	tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
	tempSubMesh->StartIndexLocation = currentStartIndexCount;
	tempSubMesh->BaseVertexLocation = currentBaseVertexLocation;

	currentStartIndexCount += tempMesh.Indices32.size();
	currentBaseVertexLocation += tempMesh.Vertices.size();

	mSubMeshes["Quad"] = std::move(tempSubMesh);

	vertices.resize(totalVertexCount);

	for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = tempMesh.Vertices[i].Position;
		vertices[k].Normal = tempMesh.Vertices[i].Normal;
		vertices[k].TexC = tempMesh.Vertices[i].TexC;
		vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
	}

	indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

	geo->DrawArgs["Quad"] = *mSubMeshes["Quad"];

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	mGeometries[geo->Name] = std::move(geo);
}

void DemoApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["gBufferWriteVS"]->GetBufferPointer()), 
		mShaders["gBufferWriteVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["gBufferWritePS"]->GetBufferPointer()),
		mShaders["gBufferWritePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 3;
	opaquePsoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	opaquePsoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	opaquePsoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	opaquePsoDesc.SampleDesc.Count = 1;
	opaquePsoDesc.SampleDesc.Quality = 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));

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
            1, (UINT)mOpaqueRObjects.size(), (UINT)mMaterials.size()));
    }
}

void DemoApp::BuildMaterials()
{
	int currentCBIndex = 0;
	int currentHeapIndex = 0;

	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		if (mMaterials.find(mScene.objectsInScene[i].meshName + "Material") == mMaterials.end())
		{
			auto mat = std::make_unique<Material>();
			mat->Name = mScene.objectsInScene[i].meshName + "Material";
			mat->MatCBIndex = currentCBIndex++;
			mat->DiffuseSrvHeapIndex = currentHeapIndex;
			mat->Metallic = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

			mMaterials[mat->Name] = std::move(mat);

			currentHeapIndex += 2;
		}
	}

	auto mat = std::make_unique<Material>();
	mat->Name = "PostProcessingMaterial";
	mat->MatCBIndex = currentCBIndex++;
	mat->DiffuseSrvHeapIndex = 0;
	mat->Metallic = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	mMaterials[mat->Name] = std::move(mat);
}

void DemoApp::BuildRenderObjects()
{
	int currentCBIndex = 0;

	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		auto rObject = std::make_unique<RenderObject>();
		rObject->SetObjCBIndex(currentCBIndex);
		rObject->SetMat(mMaterials[mScene.objectsInScene[i].meshName + "Material"].get());
		rObject->SetGeo(mGeometries[mScene.name].get());
		rObject->SetPrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rObject->SetIndexCount(mScene.objectsInScene[i].meshName);
		rObject->SetStartIndexLocation(mScene.objectsInScene[i].meshName);
		rObject->SetBaseVertexLocation(mScene.objectsInScene[i].meshName);
		rObject->SetWorldMatrix(&(XMMatrixScaling(mScene.objectsInScene[i].scale.x, mScene.objectsInScene[i].scale.y, mScene.objectsInScene[i].scale.z)
			* XMMatrixRotationQuaternion(XMLoadFloat4(&mScene.objectsInScene[i].rotation))
			* XMMatrixTranslation(mScene.objectsInScene[i].position.x, mScene.objectsInScene[i].position.y, mScene.objectsInScene[i].position.z)));

		mOpaqueRObjects.push_back(std::move(rObject));

		currentCBIndex++;
	}

	// Make the post processing quad render item
	mQuadrObject = std::make_unique<RenderObject>();
	mQuadrObject->SetWorldMatrix(&XMMatrixIdentity());
	mQuadrObject->SetObjCBIndex(currentCBIndex);
	mQuadrObject->SetMat(mMaterials["PostProcessingMaterial"].get());
	mQuadrObject->SetGeo(mGeometries[mScene.name].get());
	mQuadrObject->SetPrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mQuadrObject->SetIndexCount("Quad");
	mQuadrObject->SetStartIndexLocation("Quad");
	mQuadrObject->SetBaseVertexLocation("Quad");

}

void DemoApp::DrawRenderObjects(ID3D12GraphicsCommandList* cmdList)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    for(size_t i = 0; i < mOpaqueRObjects.size(); ++i)
    {
		mOpaqueRObjects[i]->Draw(cmdList, objectCB, matCB, mSrvDescriptorHeap, mCbvSrvDescriptorSize, objCBByteSize, matCBByteSize);
    }
}

void DemoApp::DrawPostProcessingQuad(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->IASetVertexBuffers(0, 1, &mQuadrObject->Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mQuadrObject->Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mQuadrObject->PrimitiveType);

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvPostProcessingDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex.Offset(mQuadrObject->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

	cmdList->SetGraphicsRootDescriptorTable(0, tex);

	cmdList->DrawIndexedInstanced(mQuadrObject->IndexCount, 1, mQuadrObject->StartIndexLocation, mQuadrObject->BaseVertexLocation, 0);
	
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