#include "MeshLoader.h"

using namespace DirectX;
 
MeshLoader::MeshData MeshLoader::CreateQuad()
{
    MeshData meshData;

	meshData.Vertices.resize(4);
	meshData.Indices32.resize(6);

	// Position coordinates specified in NDC space.
	meshData.Vertices[0] = Vertex(
        0.0f, -2.0f, 0.0f,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f);

	meshData.Vertices[1] = Vertex(
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f);

	meshData.Vertices[2] = Vertex(
		2.0f, 0.0f, 0.0f,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f);

	meshData.Vertices[3] = Vertex(
		2.0f, -2.0f, 0.0f,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f);

	meshData.Indices32[0] = 0;
	meshData.Indices32[1] = 1;
	meshData.Indices32[2] = 2;

	meshData.Indices32[3] = 0;
	meshData.Indices32[4] = 2;
	meshData.Indices32[5] = 3;

    return meshData;
}

MeshLoader::MeshData MeshLoader::LoadModel(std::string modelName)
{
	std::ifstream inputFile;
	inputFile.open("../Assets/Meshes/" + modelName + ".txt", std::fstream::in);

	std::string name;
	size_t numberOfVertices;
	size_t numberOfIndices;

	inputFile >> numberOfVertices;
	inputFile >> numberOfIndices;

	MeshData meshData;

	meshData.Vertices.resize(numberOfVertices);
	meshData.Indices32.resize(numberOfIndices);

	for (size_t i = 0; i < numberOfVertices; ++i)
	{
		inputFile >> meshData.Vertices[i].Position.x >> meshData.Vertices[i].Position.y >> meshData.Vertices[i].Position.z;
		inputFile >> meshData.Vertices[i].Normal.x >> meshData.Vertices[i].Normal.y >> meshData.Vertices[i].Normal.z;
		inputFile >> meshData.Vertices[i].TangentU.x >> meshData.Vertices[i].TangentU.y >> meshData.Vertices[i].TangentU.z;
		inputFile >> meshData.Vertices[i].TexC.x >> meshData.Vertices[i].TexC.y;
	}

	for (size_t i = 0; i < numberOfIndices; ++i)
	{
		inputFile >> meshData.Indices32[i];
	}

	inputFile.close();

	return meshData;
}