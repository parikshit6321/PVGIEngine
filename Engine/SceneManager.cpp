#include "SceneManager.h"

Scene SceneManager::mScene;

void SceneManager::LoadScene(std::string sceneFilePath, Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList)
{
	ImportScene(sceneFilePath);
	LoadTextures(md3dDevice, mCommandList);
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

void SceneManager::BuildSceneGeometry()
{
}

void SceneManager::BuildMaterials()
{
}

void SceneManager::BuildRenderObjects()
{
}
