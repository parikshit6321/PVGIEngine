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

	mScene.name = name;
	mScene.cameraPosition = cameraPosition;
	mScene.cameraRotation = cameraRotation;
	mScene.lightDirection = lightDirection;
	mScene.lightStrength = lightStrength;
	mScene.numberOfObjects = numberOfObjects;

	mScene.mObjectsInScene = new SceneObject[numberOfObjects];

	mScene.numberOfUniqueObjects = 0;

	std::vector<std::string> objectMeshNames;

	for (UINT i = 0; i < numberOfObjects; ++i)
	{
		inputFile >> mScene.mObjectsInScene[i].meshName;
		inputFile >> mScene.mObjectsInScene[i].diffuseOpacityTextureName;
		inputFile >> mScene.mObjectsInScene[i].normalRoughnessTextureName;
		inputFile >> mScene.mObjectsInScene[i].position.x >> mScene.mObjectsInScene[i].position.y >> mScene.mObjectsInScene[i].position.z;
		inputFile >> mScene.mObjectsInScene[i].rotation.x >> mScene.mObjectsInScene[i].rotation.y >> mScene.mObjectsInScene[i].rotation.z >> mScene.mObjectsInScene[i].rotation.w;
		inputFile >> mScene.mObjectsInScene[i].scale.x >> mScene.mObjectsInScene[i].scale.y >> mScene.mObjectsInScene[i].scale.z;

		bool isAlreadyPresent = false;

		for (size_t j = 0; j < objectMeshNames.size(); ++j)
		{
			if (objectMeshNames[j] == mScene.mObjectsInScene[i].meshName)
			{
				isAlreadyPresent = true;
				break;
			}
		}

		if (!isAlreadyPresent)
		{
			objectMeshNames.push_back(mScene.mObjectsInScene[i].meshName);
			++mScene.numberOfUniqueObjects;
		}
	}

	inputFile.close();
}

void SceneManager::ResizeBuffers()
{
	mScene.mSceneGeometry = std::make_unique<MeshGeometry>();
	mScene.mSceneGeometry->Name = mScene.name;
	
	// 1 extra for quad
	mScene.mSceneGeometry->DrawArgs = new SubmeshGeometry[mScene.numberOfUniqueObjects + 1];
	// 2 extra for skybox cubemap and lut texture
	mScene.mTextures = new std::unique_ptr<Texture>[(2 * mScene.numberOfUniqueObjects) + 2];
	mScene.mMaterials = new std::unique_ptr<Material>[mScene.numberOfUniqueObjects];
	// 1 extra for quad
	mScene.mSubMeshes = new std::unique_ptr<SubmeshGeometry>[mScene.numberOfUniqueObjects + 1];
	
	mScene.mOpaqueRObjects = new std::unique_ptr<RenderObject>[mScene.numberOfObjects];
}

void SceneManager::LoadTextures(Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	UINT index = 0;
	std::vector<std::string> textureNamesProcessed;

	// Load the scene material textures
	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		bool isAlreadyProcessed = false;
		
		for (size_t j = 0; j < textureNamesProcessed.size(); ++j)
		{
			if (textureNamesProcessed[j] == mScene.mObjectsInScene[i].diffuseOpacityTextureName)
			{
				isAlreadyProcessed = true;
				break;
			}
		}

		if (isAlreadyProcessed)
		{
			continue;
		}
		else
		{
			textureNamesProcessed.push_back(mScene.mObjectsInScene[i].diffuseOpacityTextureName);
		}

		// Load the diffuse opacity texture first
		std::wstringstream wsDiffuseOpacity;
		wsDiffuseOpacity << mScene.mObjectsInScene[i].diffuseOpacityTextureName.c_str();
		std::wstring wsNameDiffuseOpacity = wsDiffuseOpacity.str();

		auto texDiffuseOpacity = std::make_unique<Texture>();
		texDiffuseOpacity->Name = mScene.mObjectsInScene[i].diffuseOpacityTextureName;
		texDiffuseOpacity->Filename = L"../Assets/Textures/" + wsNameDiffuseOpacity + L".dds";
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texDiffuseOpacity->Filename.c_str(),
			texDiffuseOpacity->Resource, texDiffuseOpacity->UploadHeap));

		mScene.mTextures[index++] = std::move(texDiffuseOpacity);

		// Load the normal roughness texture next
		std::wstringstream wsNormalRoughness;
		wsNormalRoughness << mScene.mObjectsInScene[i].normalRoughnessTextureName.c_str();
		std::wstring wsNameNormalRoughness = wsNormalRoughness.str();

		auto texNormalRoughness = std::make_unique<Texture>();
		texNormalRoughness->Name = mScene.mObjectsInScene[i].normalRoughnessTextureName;
		texNormalRoughness->Filename = L"../Assets/Textures/" + wsNameNormalRoughness + L".dds";

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texNormalRoughness->Filename.c_str(),
			texNormalRoughness->Resource, texNormalRoughness->UploadHeap));

		mScene.mTextures[index++] = std::move(texNormalRoughness);
	}

	// Load the skybox texture
	auto texSkyBox = std::make_unique<Texture>();
	texSkyBox->Name = "SkyBox";
	texSkyBox->Filename = L"../Assets/Textures/SkyBox.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), texSkyBox->Filename.c_str(),
		texSkyBox->Resource, texSkyBox->UploadHeap));

	mScene.mTextures[index++] = std::move(texSkyBox);

	// Load the lut texture
	auto texLUT = std::make_unique<Texture>();
	texLUT->Name = "LUT";
	texLUT->Filename = L"../Assets/Textures/LUT.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), texLUT->Filename.c_str(),
		texLUT->Resource, texLUT->UploadHeap));

	mScene.mTextures[index++] = std::move(texLUT);
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
	UINT index = 0;

	std::vector<std::string> meshNamesProcessed;

	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		bool isAlreadyProcessed = false;

		for (size_t j = 0; j < meshNamesProcessed.size(); ++j)
		{
			if (meshNamesProcessed[j] == mScene.mObjectsInScene[i].meshName)
			{
				isAlreadyProcessed = true;
				break;
			}
		}

		if (isAlreadyProcessed)
		{
			continue;
		}
		else
		{
			meshNamesProcessed.push_back(mScene.mObjectsInScene[i].meshName);
		}

		MeshLoader::MeshData tempMesh = MeshLoader::LoadModel(mScene.mObjectsInScene[i].meshName);

		totalVertexCount += tempMesh.Vertices.size();

		auto tempSubMesh = std::make_unique<SubmeshGeometry>();;
		tempSubMesh->Name = mScene.mObjectsInScene[i].meshName;
		tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
		tempSubMesh->StartIndexLocation = (UINT)currentStartIndexCount;
		tempSubMesh->BaseVertexLocation = (INT)currentBaseVertexLocation;

		currentStartIndexCount += tempMesh.Indices32.size();
		currentBaseVertexLocation += tempMesh.Vertices.size();

		mScene.mSubMeshes[index] = std::move(tempSubMesh);

		vertices.resize(totalVertexCount);

		for (size_t it = 0; it < tempMesh.Vertices.size(); ++it, ++k)
		{
			vertices[k].Pos = tempMesh.Vertices[it].Position;
			vertices[k].Normal = tempMesh.Vertices[it].Normal;
			vertices[k].TexC = tempMesh.Vertices[it].TexC;
			vertices[k].Tangent = tempMesh.Vertices[it].TangentU;
		}

		indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

		mScene.mSceneGeometry->DrawArgs[index] = *mScene.mSubMeshes[index];

		++index;
	}

	// Create the post processing quad geometry
	MeshLoader::MeshData tempMesh = MeshLoader::CreateQuad();

	totalVertexCount += tempMesh.Vertices.size();

	auto tempSubMesh = std::make_unique<SubmeshGeometry>();
	tempSubMesh->Name = "Quad";
	tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
	tempSubMesh->StartIndexLocation = (UINT)currentStartIndexCount;
	tempSubMesh->BaseVertexLocation = (INT)currentBaseVertexLocation;

	currentStartIndexCount += tempMesh.Indices32.size();
	currentBaseVertexLocation += tempMesh.Vertices.size();

	mScene.mSubMeshes[index] = std::move(tempSubMesh);

	vertices.resize(totalVertexCount);

	for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = tempMesh.Vertices[i].Position;
		vertices[k].Normal = tempMesh.Vertices[i].Normal;
		vertices[k].TexC = tempMesh.Vertices[i].TexC;
		vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
	}

	indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

	mScene.mSceneGeometry->DrawArgs[index] = *mScene.mSubMeshes[index];

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
	UINT index = 0;
	std::vector<std::string> materialNamesProcessed;

	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		bool isAlreadyProcessed = false;

		for (size_t j = 0; j < materialNamesProcessed.size(); ++j)
		{
			if (materialNamesProcessed[j] == mScene.mObjectsInScene[i].meshName)
			{
				isAlreadyProcessed = true;
				break;
			}
		}

		if (isAlreadyProcessed)
		{
			continue;
		}
		else
		{
			materialNamesProcessed.push_back(mScene.mObjectsInScene[i].meshName);
		}

		auto mat = std::make_unique<Material>();
		mat->MatCBIndex = index;
		mat->DiffuseSrvHeapIndex = index * 2;
		mat->Metallic = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		mat->Name = mScene.mObjectsInScene[i].meshName;
		mScene.mMaterials[index++] = std::move(mat);
	}
}

Material* SceneManager::GetMaterial(std::string materialName)
{
	Material* result = nullptr;

	for (UINT i = 0; i < mScene.numberOfUniqueObjects; ++i)
	{
		if (mScene.mMaterials[i]->Name == materialName)
		{
			result = mScene.mMaterials[i].get();
			break;
		}
	}

	return result;
}

UINT SceneManager::GetIndexCount(std::string meshName)
{
	UINT result = 0;

	for (UINT i = 0; i < mScene.numberOfUniqueObjects; ++i)
	{
		if (mScene.mSceneGeometry->DrawArgs[i].Name == meshName)
		{
			result = mScene.mSceneGeometry->DrawArgs[i].IndexCount;
			break;
		}
	}

	return result;
}

UINT SceneManager::GetStartIndexLocation(std::string meshName)
{
	UINT result = 0;

	for (UINT i = 0; i < mScene.numberOfUniqueObjects; ++i)
	{
		if (mScene.mSceneGeometry->DrawArgs[i].Name == meshName)
		{
			result = mScene.mSceneGeometry->DrawArgs[i].StartIndexLocation;
			break;
		}
	}

	return result;
}

int SceneManager::GetBaseVertexLocation(std::string meshName)
{
	int result = 0;

	for (UINT i = 0; i < mScene.numberOfUniqueObjects; ++i)
	{
		if (mScene.mSceneGeometry->DrawArgs[i].Name == meshName)
		{
			result = mScene.mSceneGeometry->DrawArgs[i].BaseVertexLocation;
			break;
		}
	}

	return result;
}

void SceneManager::BuildRenderObjects()
{
	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		std::string meshName = mScene.mObjectsInScene[i].meshName;

		auto rObject = std::make_unique<RenderObject>();
		rObject->SetObjCBIndex(i);
		rObject->SetMat(GetMaterial(meshName));
		rObject->SetGeo(mScene.mSceneGeometry.get());
		rObject->SetPrimitiveType(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rObject->SetIndexCount(GetIndexCount(meshName));
		rObject->SetStartIndexLocation(GetStartIndexLocation(meshName));
		rObject->SetBaseVertexLocation(GetBaseVertexLocation(meshName));
		rObject->SetWorldMatrix(&(XMMatrixScaling(mScene.mObjectsInScene[i].scale.x, mScene.mObjectsInScene[i].scale.y, mScene.mObjectsInScene[i].scale.z)
			* XMMatrixRotationQuaternion(XMLoadFloat4(&mScene.mObjectsInScene[i].rotation))
			* XMMatrixTranslation(mScene.mObjectsInScene[i].position.x, mScene.mObjectsInScene[i].position.y, mScene.mObjectsInScene[i].position.z)));

		mScene.mOpaqueRObjects[i] = std::move(rObject);
	}

	// Make the post processing quad render object
	mScene.mQuadrObject = std::make_unique<RenderObject>();
	mScene.mQuadrObject->InitializeAsQuad(mScene.mSceneGeometry.get(), mScene.numberOfUniqueObjects);
}

void SceneManager::ReleaseMemory()
{
	delete[] mScene.mObjectsInScene;
	delete[] mScene.mSceneGeometry->DrawArgs;

	for (UINT i = 0; i < mScene.numberOfObjects; ++i)
	{
		mScene.mOpaqueRObjects[i].release();
	}

	for (UINT i = 0; i < (2 * mScene.numberOfUniqueObjects) + 2; ++i)
		mScene.mTextures[i].release();

	for (UINT i = 0; i < mScene.numberOfUniqueObjects; ++i)
		mScene.mMaterials[i].release();

	for (UINT i = 0; i < mScene.numberOfUniqueObjects + 1; ++i)
		mScene.mSubMeshes[i].release();

	mScene.mQuadrObject.release();
	mScene.mSceneGeometry.release();
}