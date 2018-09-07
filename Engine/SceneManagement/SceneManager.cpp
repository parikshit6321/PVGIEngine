#include "SceneManager.h"

Scene SceneManager::mScene;

void SceneManager::LoadScene(std::string sceneFilePath, Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	ImportScene(sceneFilePath);
	ResizeBuffers();
	LoadTextures(md3dDevice, mCommandList);
	BuildSceneGeometry(md3dDevice, mCommandList);
	BuildMaterials();
	BuildRenderObjects();
}

Scene* SceneManager::GetScenePtr()
{
	return &mScene;
}

void SceneManager::ImportScene(std::string sceneFilePath)
{
	std::ifstream inputFile;
	inputFile.open(sceneFilePath, std::fstream::in);

	std::string name;
	DirectX::XMFLOAT3 cameraPosition;
	DirectX::XMFLOAT4 cameraRotation;
	DirectX::XMFLOAT3 lightDirection;
	DirectX::XMFLOAT3 lightStrength;
	UINT numberOfObjects;

	inputFile >> name;
	inputFile >> cameraPosition.x >> cameraPosition.y >> cameraPosition.z;
	inputFile >> cameraRotation.x >> cameraRotation.y >> cameraRotation.z >> cameraRotation.w;
	inputFile >> lightDirection.x >> lightDirection.y >> lightDirection.z;
	inputFile >> lightStrength.x >> lightStrength.y >> lightStrength.z;
	inputFile >> numberOfObjects;

	cameraRotation.x *= -1.0f;
	cameraRotation.y *= -1.0f;
	cameraRotation.z *= -1.0f;

	mScene.name = name;
	mScene.cameraPosition = cameraPosition;
	mScene.cameraRotation = cameraRotation;
	mScene.lightDirection = lightDirection;
	mScene.lightStrength = lightStrength;
	mScene.numberOfObjects = numberOfObjects;

	mScene.objectsInScene = new SceneObject[numberOfObjects];

	for (UINT i = 0; i < numberOfObjects; ++i)
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

void SceneManager::ResizeBuffers()
{
	mScene.mSceneGeometry = std::make_unique<MeshGeometry>();
	mScene.mSceneGeometry->Name = mScene.name;
	mScene.mSceneGeometry->DrawArgs = new SubmeshGeometry[mScene.numberOfObjects + 1];
	mScene.mTextures = new std::unique_ptr<Texture>[(2 * mScene.numberOfObjects) + 1];
	mScene.mMaterials = new std::unique_ptr<Material>[mScene.numberOfObjects];
	mScene.mSubMeshes = new std::unique_ptr<SubmeshGeometry>[mScene.numberOfObjects + 1];
	mScene.mOpaqueRObjects = new std::unique_ptr<RenderObject>[mScene.numberOfObjects];
}

void SceneManager::LoadTextures(Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	// Load the scene material textures
	for (UINT i = 0; i < (2 * mScene.numberOfObjects); i += 2)
	{
		// Load the diffuse opacity texture first
		std::wstringstream wsDiffuseOpacity;
		wsDiffuseOpacity << mScene.objectsInScene[i/2].diffuseOpacityTextureName.c_str();
		std::wstring wsNameDiffuseOpacity = wsDiffuseOpacity.str();

		auto texDiffuseOpacity = std::make_unique<Texture>();
		texDiffuseOpacity->Name = mScene.objectsInScene[i/2].diffuseOpacityTextureName;
		texDiffuseOpacity->Filename = L"../Assets/Textures/" + wsNameDiffuseOpacity + L".dds";
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texDiffuseOpacity->Filename.c_str(),
			texDiffuseOpacity->Resource, texDiffuseOpacity->UploadHeap));

		mScene.mTextures[i] = std::move(texDiffuseOpacity);

		// Load the normal roughness texture next
		std::wstringstream wsNormalRoughness;
		wsNormalRoughness << mScene.objectsInScene[i/2].normalRoughnessTextureName.c_str();
		std::wstring wsNameNormalRoughness = wsNormalRoughness.str();

		auto texNormalRoughness = std::make_unique<Texture>();
		texNormalRoughness->Name = mScene.objectsInScene[i/2].normalRoughnessTextureName;
		texNormalRoughness->Filename = L"../Assets/Textures/" + wsNameNormalRoughness + L".dds";

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texNormalRoughness->Filename.c_str(),
			texNormalRoughness->Resource, texNormalRoughness->UploadHeap));

		mScene.mTextures[i+1] = std::move(texNormalRoughness);
	}

	// Load the skybox texture
	auto texSkyBox = std::make_unique<Texture>();
	texSkyBox->Name = "SkyBox";
	texSkyBox->Filename = L"../Assets/Textures/SkyBox.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), texSkyBox->Filename.c_str(),
		texSkyBox->Resource, texSkyBox->UploadHeap));

	mScene.mTextures[2 * mScene.numberOfObjects] = std::move(texSkyBox);
}

void SceneManager::BuildSceneGeometry(Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	size_t totalVertexCount = 0;
	size_t currentStartIndexCount = 0;
	size_t currentBaseVertexLocation = 0;

	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;

	UINT k = 0;

	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		MeshLoader::MeshData tempMesh = MeshLoader::LoadModel(mScene.objectsInScene[i].meshName);

		totalVertexCount += tempMesh.Vertices.size();

		auto tempSubMesh = std::make_unique<SubmeshGeometry>();;
		tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
		tempSubMesh->StartIndexLocation = currentStartIndexCount;
		tempSubMesh->BaseVertexLocation = currentBaseVertexLocation;

		currentStartIndexCount += tempMesh.Indices32.size();
		currentBaseVertexLocation += tempMesh.Vertices.size();

		mScene.mSubMeshes[i] = std::move(tempSubMesh);

		vertices.resize(totalVertexCount);

		for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
		{
			vertices[k].Pos = tempMesh.Vertices[i].Position;
			vertices[k].Normal = tempMesh.Vertices[i].Normal;
			vertices[k].TexC = tempMesh.Vertices[i].TexC;
			vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
		}

		indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

		mScene.mSceneGeometry->DrawArgs[i] = *mScene.mSubMeshes[i];
	}

	// Create the post processing quad geometry
	MeshLoader::MeshData tempMesh = MeshLoader::CreateQuad();

	totalVertexCount += tempMesh.Vertices.size();

	auto tempSubMesh = std::make_unique<SubmeshGeometry>();;
	tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
	tempSubMesh->StartIndexLocation = currentStartIndexCount;
	tempSubMesh->BaseVertexLocation = currentBaseVertexLocation;

	currentStartIndexCount += tempMesh.Indices32.size();
	currentBaseVertexLocation += tempMesh.Vertices.size();

	mScene.mSubMeshes[mScene.numberOfObjects] = std::move(tempSubMesh);

	vertices.resize(totalVertexCount);

	for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = tempMesh.Vertices[i].Position;
		vertices[k].Normal = tempMesh.Vertices[i].Normal;
		vertices[k].TexC = tempMesh.Vertices[i].TexC;
		vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
	}

	indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

	mScene.mSceneGeometry->DrawArgs[mScene.numberOfObjects] = *mScene.mSubMeshes[mScene.numberOfObjects];

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mScene.mSceneGeometry->VertexBufferCPU));
	CopyMemory(mScene.mSceneGeometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mScene.mSceneGeometry->IndexBufferCPU));
	CopyMemory(mScene.mSceneGeometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mScene.mSceneGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mScene.mSceneGeometry->VertexBufferUploader);

	mScene.mSceneGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mScene.mSceneGeometry->IndexBufferUploader);

	mScene.mSceneGeometry->VertexByteStride = sizeof(Vertex);
	mScene.mSceneGeometry->VertexBufferByteSize = vbByteSize;
	mScene.mSceneGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
	mScene.mSceneGeometry->IndexBufferByteSize = ibByteSize;
}

void SceneManager::BuildMaterials()
{
	int currentCBIndex = 0;
	int currentHeapIndex = 0;

	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		auto mat = std::make_unique<Material>();
		mat->MatCBIndex = currentCBIndex++;
		mat->DiffuseSrvHeapIndex = currentHeapIndex;
		mat->Metallic = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		mScene.mMaterials[i] = std::move(mat);

		currentHeapIndex += 2;
	}
}

void SceneManager::BuildRenderObjects()
{
	int currentCBIndex = 0;

	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		auto rObject = std::make_unique<RenderObject>();
		rObject->SetObjCBIndex(currentCBIndex);
		rObject->SetMat(mScene.mMaterials[i].get());
		rObject->SetGeo(mScene.mSceneGeometry.get());
		rObject->SetPrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rObject->SetIndexCount(mScene.mSceneGeometry.get()->DrawArgs[i].IndexCount);
		rObject->SetStartIndexLocation(mScene.mSceneGeometry.get()->DrawArgs[i].StartIndexLocation);
		rObject->SetBaseVertexLocation(mScene.mSceneGeometry.get()->DrawArgs[i].BaseVertexLocation);
		rObject->SetWorldMatrix(&(XMMatrixScaling(mScene.objectsInScene[i].scale.x, mScene.objectsInScene[i].scale.y, mScene.objectsInScene[i].scale.z)
			* XMMatrixRotationQuaternion(XMLoadFloat4(&mScene.objectsInScene[i].rotation))
			* XMMatrixTranslation(mScene.objectsInScene[i].position.x, mScene.objectsInScene[i].position.y, mScene.objectsInScene[i].position.z)));

		mScene.mOpaqueRObjects[i] = std::move(rObject);

		currentCBIndex++;
	}

	// Make the post processing quad render object
	mScene.mQuadrObject = std::make_unique<RenderObject>();
	mScene.mQuadrObject->InitializeAsQuad(mScene.mSceneGeometry.get(), mScene.numberOfObjects);
}