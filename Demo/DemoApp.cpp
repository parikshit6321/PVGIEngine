#include "../Engine/Utilities/Camera.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

/// <summary>
/// DemoApp class is the main application class which dervices from the D3DApp class and extends it's update, draw and input functions.
/// </summary>
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

	virtual void OnKeyPress(WPARAM keyState)override;

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

	Camera mCamera;
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

/// <summary>
/// Entry-point of the main application.
/// </summary>
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

/// <summary>
/// Function which loads the scene, initializes the renderer, builds the frame resources and executes the initial command lists.
/// </summary>
bool DemoApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Load the scene file
	SceneManager::LoadScene("../Assets/Scenes/DemoScene4.txt", md3dDevice, mCommandList);
	// Initialize the renderer
	Renderer::Initialize(md3dDevice, mClientWidth, mClientHeight, mBackBufferFormat, mDepthStencilFormat);
	// Build the frame resources
	BuildFrameResources();
	
	// Initialize the game camera.
	mCamera.Initialize(XMFLOAT4(SceneManager::GetScenePtr()->cameraPosition.x,
		SceneManager::GetScenePtr()->cameraPosition.y,
		SceneManager::GetScenePtr()->cameraPosition.z,
		1.0f));

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
/// <summary>
/// Resize function (currently not implemented dynamic resizing)
/// </summary>
void DemoApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.33f*MathHelper::Pi, AspectRatio(), 1.0f, 500.0f);
    XMStoreFloat4x4(&mProj, P);
}

/// <summary>
/// Function which accepts input, updates the camera and the 3 constant buffers.
/// </summary>
void DemoApp::Update(const GameTimer& gt)
{
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

	// Update the 3 constant buffers
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

/// <summary>
/// Function which executes the individual render passes one by one
/// </summary>
void DemoApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), Renderer::directLightingRenderPass.mPSO.Get()));

	// Render shadows only once
	if (Renderer::bPerformShadowMapping)
	{
		// Draw shadows
		Renderer::shadowMapRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), mCurrFrameResource);
		Renderer::bPerformShadowMapping = false;
	}
	
	// Reset viewports and scissor rects after shadow map render passs
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Clear the depth buffer at the beginning of every frame
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Execute all the render passes
	Renderer::Execute(mCommandList.Get(), &DepthStencilView(), mCurrFrameResource);

	// Copy the contents of the off-screen texture to the back buffer
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

/// <summary>
/// Camera keyboard input
/// </summary>
void DemoApp::OnKeyPress(WPARAM keyState)
{
	// W pressed
	if (keyState == 0x57)
	{
		mCamera.MoveForward(mTimer.DeltaTime());
	}
	// S pressed
	else if (keyState == 0x53)
	{
		mCamera.MoveBackward(mTimer.DeltaTime());
	}
	// A pressed
	else if (keyState == 0x41)
	{
		mCamera.MoveLeft(mTimer.DeltaTime());
	}
	// D pressed
	else if (keyState == 0x44)
	{
		mCamera.MoveRight(mTimer.DeltaTime());
	}
	// Q pressed
	else if (keyState == 0x51)
	{
		mCamera.MoveUp(mTimer.DeltaTime());
	}
	// E pressed
	else if (keyState == 0x45)
	{
		mCamera.MoveDown(mTimer.DeltaTime());
	}
	// Right arrow pressed
	else if (keyState == VK_RIGHT)
	{
		mCamera.RotateRight(mTimer.DeltaTime());
	}
	// Left arrow pressed
	else if (keyState == VK_LEFT)
	{
		mCamera.RotateLeft(mTimer.DeltaTime());
	}
	// Up arrow pressed
	else if (keyState == VK_UP)
	{
		mCamera.RotateUp(mTimer.DeltaTime());
	}
	// Down arrow pressed
	else if (keyState == VK_DOWN)
	{
		mCamera.RotateDown(mTimer.DeltaTime());
	}	
}

/// <summary>
/// Updates the camera position and rotation and generates the view matrix (currently static camera)
/// </summary>
void DemoApp::UpdateCamera(const GameTimer& gt)
{
	// Build the view matrix.
	XMVECTOR pos = XMLoadFloat4(mCamera.GetPositionPtr());
	XMVECTOR target = XMLoadFloat4(mCamera.GetTargetPtr());
	XMVECTOR up = XMLoadFloat4(mCamera.GetUpDirectionPtr());

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

/// <summary>
/// Update the object constant buffer (currently only has worldSpace matrix)
/// </summary>
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

/// <summary>
/// Updates the material constant buffer (currently only has a global metallic flag)
/// </summary>
void DemoApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(UINT i = 0; i < SceneManager::GetScenePtr()->numberOfUniqueObjects; ++i)
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

/// <summary>
/// Updates the main pass constant buffer which contains the bulk of the constant data
/// </summary>
void DemoApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	// Update view, projection and related matrices
	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	// Camera position
	mMainPassCB.EyePosW = XMFLOAT3(mCamera.GetPositionPtr()->x, mCamera.GetPositionPtr()->y, mCamera.GetPositionPtr()->z);
	
	// LUT contribution for color grading pass
	mMainPassCB.userLUTContribution = 1.0f;

	// Render target size
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	
	// Near and far planes for the camera
	mMainPassCB.NearZ = 0.1f;
	mMainPassCB.FarZ = 500.0f;

	// Time parameters (maybe use later for vertex animation)
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	// RGB - sunlight color; A - sunlight intensity
	mMainPassCB.SunLightStrength = { SceneManager::GetScenePtr()->lightStrength.x, 
									 SceneManager::GetScenePtr()->lightStrength.y, 
									 SceneManager::GetScenePtr()->lightStrength.z, 10.0f };

	// RGB - direction of sunlight; A - unused
	mMainPassCB.SunLightDirection = { SceneManager::GetScenePtr()->lightDirection.x, 
									  SceneManager::GetScenePtr()->lightDirection.y,
									  SceneManager::GetScenePtr()->lightDirection.z, 0.0f };

	// Matrix used when rendering skybox using a quad
	XMFLOAT4 skyboxTranslationRow = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	
	XMStoreFloat4x4(&mMainPassCB.skyBoxMatrix, XMMatrixTranspose(
		XMMATRIX(XMLoadFloat4(mCamera.GetRightDirectionPtr()), XMLoadFloat4(mCamera.GetUpDirectionPtr()),
		XMLoadFloat4(mCamera.GetForwardDirectionPtr()), XMLoadFloat4(&skyboxTranslationRow))));

	// Only the sun light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat4(&mMainPassCB.SunLightDirection);
	XMVECTOR lightPos = -2.0f* 10.0f *lightDir;
	XMFLOAT4 targetPosTemp = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR targetPos = XMLoadFloat4(&targetPosTemp);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum for the shadow mapping pass.
	float l = sphereCenterLS.x - 10.0f;
	float b = sphereCenterLS.y - 10.0f;
	float n = sphereCenterLS.z - 10.0f;
	float r = sphereCenterLS.x + 10.0f;
	float t = sphereCenterLS.y + 10.0f;
	float f = sphereCenterLS.z + 10.0f;

	// Light orthographic projection matrix
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, 10.0f, 40.0f);

	// View projection matrix for shadow map render pass
	XMMATRIX shadowViewProjMatrix = lightView * lightProj;

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = shadowViewProjMatrix * T;
	
	XMStoreFloat4x4(&mMainPassCB.shadowViewProjMatrix, XMMatrixTranspose(shadowViewProjMatrix));
	XMStoreFloat4x4(&mMainPassCB.shadowTransform, XMMatrixTranspose(S));

	float lengthOfCone = (32.0f * Renderer::voxelInjectionRenderPass.worldVolumeBoundary) / ((Renderer::voxelInjectionRenderPass.voxelResolution / 2) * tan(MathHelper::Pi / 6.0f));

	mMainPassCB.worldBoundary_R_ConeStep_G_HalfCellWidth_B = { Renderer::voxelInjectionRenderPass.worldVolumeBoundary, 
															   (lengthOfCone / 64.0f), 
															   (Renderer::voxelInjectionRenderPass.worldVolumeBoundary / 
															   Renderer::shIndirectRenderPass.gridResolution), 0.0f };

	mMainPassCB.SunLightDirection.x *= -1.0f;
	mMainPassCB.SunLightDirection.y *= -1.0f;
	mMainPassCB.SunLightDirection.z *= -1.0f;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

/// <summary>
/// Create the frame resources
/// </summary>
void DemoApp::BuildFrameResources()
{
    for(int i = 0; i < 3; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)SceneManager::GetScenePtr()->numberOfObjects, (UINT)SceneManager::GetScenePtr()->numberOfUniqueObjects));
    }
}