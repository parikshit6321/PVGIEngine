#pragma once

#include "../Common/d3dUtil.h"
#include "FrameResource.h"
#include "../Common/MeshLoader.h"

struct SceneObject
{
	SceneObject() = default;

	std::string					meshName;
	std::string					diffuseOpacityTextureName;
	std::string					normalRoughnessTextureName;
	DirectX::XMFLOAT3			position;
	DirectX::XMFLOAT4			rotation;
	DirectX::XMFLOAT3			scale;
};

struct Scene
{
	Scene() = default;

	std::string					name;
	DirectX::XMFLOAT3			cameraPosition;
	DirectX::XMFLOAT3			lightDirection;
	DirectX::XMFLOAT3			lightStrength;
	UINT						numberOfObjects;

	SceneObject*				objectsInScene;

	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<SubmeshGeometry>> mSubMeshes;

	std::unique_ptr<MeshGeometry> mSceneGeometry;

	UINT mNumTexturesLoaded = 0;
};

class SceneManager
{
public:
	SceneManager() = default;

	static void LoadScene(std::string, Microsoft::WRL::ComPtr<ID3D12Device>, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>);
	static Scene* GetScenePtr();

	~SceneManager() = default;

private:

	static void ImportScene(std::string);
	static void LoadTextures(Microsoft::WRL::ComPtr<ID3D12Device>, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>);
	static void BuildSceneGeometry(Microsoft::WRL::ComPtr<ID3D12Device>, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>);
	static void BuildMaterials();
	static void BuildRenderObjects();

	static Scene mScene;
};