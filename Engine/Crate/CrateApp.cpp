//***************************************************************************************
// CrateApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

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

class CrateApp : public D3DApp
{
public:
    CrateApp(HINSTANCE hInstance);
    CrateApp(const CrateApp& rhs) = delete;
    CrateApp& operator=(const CrateApp& rhs) = delete;
    ~CrateApp();

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
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawPostProcessingQuad(ID3D12GraphicsCommandList* cmdList);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, std::unique_ptr<SubmeshGeometry>> mSubMeshes;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
	ComPtr<ID3D12PipelineState> mPostProcessingPSO = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Post processing quad render item
	std::unique_ptr<RenderItem> mQuadRItem;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

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
        CrateApp theApp(hInstance);
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

CrateApp::CrateApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

CrateApp::~CrateApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool CrateApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadScene("../../Assets/Scenes/DemoScene2.txt");
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildMaterials();
    BuildRenderItems();
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
 
void CrateApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.33f*MathHelper::Pi, AspectRatio(), 0.1f, 500.0f);
    XMStoreFloat4x4(&mProj, P);
}

void CrateApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
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

void CrateApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPostProcessingPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    //DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

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

void CrateApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    SetCapture(mhMainWnd);
}

void CrateApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void CrateApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        
    }

}
 
void CrateApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
void CrateApp::UpdateCamera(const GameTimer& gt)
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

void CrateApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void CrateApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Metallic = mat->Metallic;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void CrateApp::UpdateMainPassCB(const GameTimer& gt)
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

void CrateApp::LoadScene(std::string sceneFilePath)
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

void CrateApp::LoadTextures()
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
			tex->Filename = L"../../Assets/Textures/" + wsName + L".dds";
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
			tex->Filename = L"../../Assets/Textures/" + wsName + L".dds";
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				mCommandList.Get(), tex->Filename.c_str(),
				tex->Resource, tex->UploadHeap));

			mTextures[tex->Name] = std::move(tex);

			++mNumTexturesLoaded;
		}
	}
}

void CrateApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

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
}

void CrateApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mNumTexturesLoaded + 3;
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

	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.Texture2D.MipLevels = 1;

	for (int i = 0; i < 3; ++i)
	{
		md3dDevice->CreateShaderResourceView(mGBuffers[i].Get(), &srvDesc, hDescriptor);

		if (i != 2)
			hDescriptor.Offset(1, mCbvSrvDescriptorSize);
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
	
}

void CrateApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["postProcessingVS"] = d3dUtil::CompileShader(L"Shaders\\PostProcessing.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["postProcessingPS"] = d3dUtil::CompileShader(L"Shaders\\PostProcessing.hlsl", nullptr, "PS", "ps_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}

void CrateApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	
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
			GeometryGenerator::MeshData tempMesh = geoGen.LoadModel(mScene.objectsInScene[i].meshName);

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
	GeometryGenerator::MeshData tempMesh = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

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

void CrateApp::BuildPSOs()
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
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
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
	postProcessingPsoDesc.pRootSignature = mRootSignature.Get();
	postProcessingPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["postProcessingVS"]->GetBufferPointer()),
		mShaders["postProcessingVS"]->GetBufferSize()
	};
	postProcessingPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["postProcessingPS"]->GetBufferPointer()),
		mShaders["postProcessingPS"]->GetBufferSize()
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

void CrateApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void CrateApp::BuildMaterials()
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
			mat->DiffuseSrvHeapIndex = currentHeapIndex++;
			mat->NormalSrvHeapIndex = currentHeapIndex++;
			mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
			mat->Metallic = 0.0f;

			mMaterials[mat->Name] = std::move(mat);
		}
	}

	auto mat = std::make_unique<Material>();
	mat->Name = "PostProcessingMaterial";
	mat->MatCBIndex = currentCBIndex++;
	mat->DiffuseSrvHeapIndex = currentHeapIndex++;
	mat->NormalSrvHeapIndex = currentHeapIndex++;
	mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	mat->Metallic = 0.0f;

	mMaterials[mat->Name] = std::move(mat);
}

void CrateApp::BuildRenderItems()
{
	int currentCBIndex = 0;

	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		auto rItem = std::make_unique<RenderItem>();
		rItem->ObjCBIndex = currentCBIndex++;
		rItem->Mat = mMaterials[mScene.objectsInScene[i].meshName + "Material"].get();
		rItem->Geo = mGeometries[mScene.name].get();
		rItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rItem->IndexCount = rItem->Geo->DrawArgs[mScene.objectsInScene[i].meshName].IndexCount;
		rItem->StartIndexLocation = rItem->Geo->DrawArgs[mScene.objectsInScene[i].meshName].StartIndexLocation;
		rItem->BaseVertexLocation = rItem->Geo->DrawArgs[mScene.objectsInScene[i].meshName].BaseVertexLocation;
		
		XMStoreFloat4x4(&(rItem->World), 
			XMMatrixScaling(mScene.objectsInScene[i].scale.x, mScene.objectsInScene[i].scale.y, mScene.objectsInScene[i].scale.z)
			* XMMatrixRotationQuaternion(XMLoadFloat4(&mScene.objectsInScene[i].rotation))
			* XMMatrixTranslation(mScene.objectsInScene[i].position.x, mScene.objectsInScene[i].position.y, mScene.objectsInScene[i].position.z));

		mAllRitems.push_back(std::move(rItem));
	}

	// All the render items are opaque.
	for (auto& e : mAllRitems)
	{
		mOpaqueRitems.push_back(e.get());
	}

	// Make the post processing quad render item
	mQuadRItem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&mQuadRItem->World, XMMatrixIdentity());
	XMStoreFloat4x4(&mQuadRItem->TexTransform, XMMatrixIdentity());
	mQuadRItem->ObjCBIndex = currentCBIndex;
	mQuadRItem->Mat = mMaterials["PostProcessingMaterial"].get();
	mQuadRItem->Geo = mGeometries[mScene.name].get();
	mQuadRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mQuadRItem->IndexCount = mQuadRItem->Geo->DrawArgs["Quad"].IndexCount;
	mQuadRItem->StartIndexLocation = mQuadRItem->Geo->DrawArgs["Quad"].StartIndexLocation;
	mQuadRItem->BaseVertexLocation = mQuadRItem->Geo->DrawArgs["Quad"].BaseVertexLocation;

}

void CrateApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void CrateApp::DrawPostProcessingQuad(ID3D12GraphicsCommandList* cmdList)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	cmdList->IASetVertexBuffers(0, 1, &mQuadRItem->Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mQuadRItem->Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mQuadRItem->PrimitiveType);

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex.Offset(mQuadRItem->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

	//D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + mQuadRItem->ObjCBIndex*objCBByteSize;
	//D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mQuadRItem->Mat->MatCBIndex*matCBByteSize;

	cmdList->SetGraphicsRootDescriptorTable(0, tex);
	//cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
	//cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

	cmdList->DrawIndexedInstanced(mQuadRItem->IndexCount, 1, mQuadRItem->StartIndexLocation, mQuadRItem->BaseVertexLocation, 0);
	
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> CrateApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}