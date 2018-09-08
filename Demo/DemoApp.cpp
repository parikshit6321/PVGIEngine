#include "../Engine/Renderer/Renderer.h"

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

	void BuildFrameResources();
    
private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

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

	SceneManager::LoadScene("../Assets/Scenes/DemoScene4.txt", md3dDevice, mCommandList);
	Renderer::Initialize(md3dDevice, mClientWidth, mClientHeight, mBackBufferFormat, mDepthStencilFormat);
	BuildFrameResources();
	
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
	auto passCB = mCurrFrameResource->PassCB->Resource();
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), Renderer::gBufferRenderPass.mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	
	Renderer::gBufferRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	Renderer::deferredShadingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	Renderer::skyBoxRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	Renderer::toneMappingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	Renderer::colorGradingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	Renderer::CopyToBackBuffer(mCommandList.Get(), CurrentBackBuffer());

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
	for(UINT i = 0; i < SceneManager::GetScenePtr()->numberOfObjects; ++i)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(SceneManager::GetScenePtr()->mOpaqueRObjects[i]->GetNumFramesDirty() > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(SceneManager::GetScenePtr()->mOpaqueRObjects[i]->GetWorldMatrixPtr());
			
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			
			currObjectCB->CopyData(SceneManager::GetScenePtr()->mOpaqueRObjects[i]->GetObjCBIndex(), objConstants);

			// Next FrameResource need to be updated too.
			SceneManager::GetScenePtr()->mOpaqueRObjects[i]->DecrementNumFramesDirty();
		}
	}
}

void DemoApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(UINT i = 0; i < SceneManager::GetScenePtr()->numberOfObjects; ++i)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		if(SceneManager::GetScenePtr()->mMaterials[i]->NumFramesDirty > 0)
		{
			MaterialConstants matConstants;
			matConstants.Metallic = SceneManager::GetScenePtr()->mMaterials[i]->Metallic;
			currMaterialCB->CopyData(SceneManager::GetScenePtr()->mMaterials[i]->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			SceneManager::GetScenePtr()->mMaterials[i]->NumFramesDirty--;
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
	XMStoreFloat4x4(&mMainPassCB.skyBoxMatrix, XMMatrixRotationQuaternion(XMLoadFloat4(&SceneManager::GetScenePtr()->cameraRotation)));
	mMainPassCB.userLUTParams = { (1.0f / 256.0f), (1.0f / 16.0f), 15.0f, 1.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DemoApp::BuildFrameResources()
{
    for(int i = 0; i < 3; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)SceneManager::GetScenePtr()->numberOfObjects, (UINT)SceneManager::GetScenePtr()->numberOfObjects));
    }
}