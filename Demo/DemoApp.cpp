#include "../Engine/Renderer/Renderer.h"

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

	bool mShadowsDrawn = false;
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
	auto passCB = mCurrFrameResource->PassCB->Resource();
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), Renderer::gBufferRenderPass.mPSO.Get()));

	// Draw shadows only once
	if (!mShadowsDrawn)
	{
		Renderer::shadowMapRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);
		mShadowsDrawn = true;
	}

	// Reset viewports and scissor rects after shadow map render passs
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Clear the depth buffer at the beginning of every frame
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Render the gBuffers
	Renderer::gBufferRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	// Compute the lighting using deferred shading
	Renderer::deferredShadingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	// Inject lighting data into the voxel grids
	Renderer::voxelInjectionRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	// Perform cone tracing to compute indirect lighting
	Renderer::indirectLightingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	// Render skybox on the background pixels using a quad
	Renderer::skyBoxRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	// Bring the texture down to LDR range from HDR using Uncharted 2 style tonemapping
	Renderer::toneMappingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

	// Use 2D LUTs for color grading
	Renderer::colorGradingRenderPass.Execute(mCommandList.Get(), &DepthStencilView(), passCB, objectCB, matCB);

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
/// Mouse down input handler (currently not implemented)
/// </summary>
void DemoApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    SetCapture(mhMainWnd);
}

/// <summary>
/// Mouse up input handler (currently not implemented)
/// </summary>
void DemoApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

/// <summary>
/// Mouse move input handler (currently not implemented)
/// </summary>
void DemoApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        
    }

}
 
/// <summary>
/// Keyboard input handler (currently not implemented)
/// </summary>
void DemoApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
/// <summary>
/// Updates the camera position and rotation and generates the view matrix (currently static camera)
/// </summary>
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
	mMainPassCB.EyePosW = mEyePos;
	
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
									 SceneManager::GetScenePtr()->lightStrength.z, 9.0f };

	// RGB - direction of sunlight; A - unused
	mMainPassCB.SunLightDirection = { SceneManager::GetScenePtr()->lightDirection.x, 
									  SceneManager::GetScenePtr()->lightDirection.y, 
									  SceneManager::GetScenePtr()->lightDirection.z, 1.0f };

	// Matrix used when rendering skybox using a quad
	XMStoreFloat4x4(&mMainPassCB.skyBoxMatrix, XMMatrixTranspose(XMMatrixRotationQuaternion(XMLoadFloat4(&SceneManager::GetScenePtr()->cameraRotation))));
	
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
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, 1.0f, 20.0f);

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

	float lengthOfCone = (32.0f * Renderer::voxelInjectionRenderPass.worldVolumeBoundary) / (Renderer::voxelInjectionRenderPass.voxelResolution * tan(MathHelper::Pi / 6.0f));

	mMainPassCB.WB_MI_LC_U = { Renderer::voxelInjectionRenderPass.worldVolumeBoundary, 64.0f, lengthOfCone, 0.0f };

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