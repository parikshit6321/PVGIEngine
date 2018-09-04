#include "SceneManager.h"

Scene SceneManager::mScene;

void SceneManager::LoadScene(std::string sceneFilePath, Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	ImportScene(sceneFilePath);
	LoadTextures(md3dDevice, mCommandList);
	BuildSceneGeometry(md3dDevice, mCommandList);
	BuildMaterials();
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
	DirectX::XMFLOAT3 lightDirection;
	DirectX::XMFLOAT3 lightStrength;
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

void SceneManager::LoadTextures(Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		if (mScene.mTextures.find(mScene.objectsInScene[i].diffuseOpacityTextureName) == mScene.mTextures.end())
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

			mScene.mTextures[tex->Name] = std::move(tex);

			++mScene.mNumTexturesLoaded;
		}

		if (mScene.mTextures.find(mScene.objectsInScene[i].normalRoughnessTextureName) == mScene.mTextures.end())
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

			mScene.mTextures[tex->Name] = std::move(tex);

			++mScene.mNumTexturesLoaded;
		}
	}
}

void SceneManager::BuildSceneGeometry(Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
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
		if (mScene.mSubMeshes.find(mScene.objectsInScene[i].meshName) == mScene.mSubMeshes.end())
		{
			MeshLoader::MeshData tempMesh = MeshLoader::LoadModel(mScene.objectsInScene[i].meshName);

			totalVertexCount += tempMesh.Vertices.size();

			auto tempSubMesh = std::make_unique<SubmeshGeometry>();;
			tempSubMesh->IndexCount = (UINT)tempMesh.Indices32.size();
			tempSubMesh->StartIndexLocation = currentStartIndexCount;
			tempSubMesh->BaseVertexLocation = currentBaseVertexLocation;

			currentStartIndexCount += tempMesh.Indices32.size();
			currentBaseVertexLocation += tempMesh.Vertices.size();

			mScene.mSubMeshes[mScene.objectsInScene[i].meshName] = std::move(tempSubMesh);

			vertices.resize(totalVertexCount);

			for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
			{
				vertices[k].Pos = tempMesh.Vertices[i].Position;
				vertices[k].Normal = tempMesh.Vertices[i].Normal;
				vertices[k].TexC = tempMesh.Vertices[i].TexC;
				vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
			}

			indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

			geo->DrawArgs[mScene.objectsInScene[i].meshName] = *mScene.mSubMeshes[mScene.objectsInScene[i].meshName];
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

	mScene.mSubMeshes["Quad"] = std::move(tempSubMesh);

	vertices.resize(totalVertexCount);

	for (size_t i = 0; i < tempMesh.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = tempMesh.Vertices[i].Position;
		vertices[k].Normal = tempMesh.Vertices[i].Normal;
		vertices[k].TexC = tempMesh.Vertices[i].TexC;
		vertices[k].Tangent = tempMesh.Vertices[i].TangentU;
	}

	indices.insert(indices.end(), std::begin(tempMesh.GetIndices16()), std::end(tempMesh.GetIndices16()));

	geo->DrawArgs["Quad"] = *mScene.mSubMeshes["Quad"];

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

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

	mScene.mSceneGeometry = std::move(geo);
}

void SceneManager::BuildMaterials()
{
	int currentCBIndex = 0;
	int currentHeapIndex = 0;

	for (int i = 0; i < mScene.numberOfObjects; ++i)
	{
		if (mScene.mMaterials.find(mScene.objectsInScene[i].meshName + "Material") == mScene.mMaterials.end())
		{
			auto mat = std::make_unique<Material>();
			mat->Name = mScene.objectsInScene[i].meshName + "Material";
			mat->MatCBIndex = currentCBIndex++;
			mat->DiffuseSrvHeapIndex = currentHeapIndex;
			mat->Metallic = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

			mScene.mMaterials[mat->Name] = std::move(mat);

			currentHeapIndex += 2;
		}
	}
}

void SceneManager::BuildRenderObjects()
{
}
