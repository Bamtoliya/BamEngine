#pragma once

#include "ResourceFilter.h"
#include "ResourceManager.h"
#include "Model.h"

void ResourceFilter::Draw()
{
	if (!m_Open) return;

	ImGui::Begin("Resource Filter", &m_Open);
	//ImGui::InputText("Filter", &m_Filter);
	ImGui::End();
}

void ResourceFilter::ImportModel(wstring filepath)
{
	filesystem::path filePath(filepath);
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(filePath.string(), aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		fmt::print("Error importing model: {}\n", importer.GetErrorString());
		return;
	}

	tagModelCreateDesc modelCreateDesc;
	ProcessModelNode(scene->mRootNode, scene, modelCreateDesc);
}

void ResourceFilter::ProcessModelNode(const aiNode* node, const aiScene* scene, tagModelCreateDesc& modelCreateDesc)
{
	vector<Mesh*> meshes;
	ResourceManager& resourceManager = ResourceManager::Get();
	for (uint32 i = 0; i < node->mNumMeshes; ++i)
	{
		tagMeshCreateInfo meshCreateInfo;
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMeshVertex(mesh, scene, meshCreateInfo);
		if (mesh->HasBones())
		{
			ProcessMeshSkinData(mesh, scene, meshCreateInfo);
		}
		ProcessMeshIndex(mesh, scene, meshCreateInfo);

		resourceManager.LoadMesh(StrToWStr(mesh->mName.C_Str()), &meshCreateInfo);
		meshes.push_back(resourceManager.GetMesh(StrToWStr(mesh->mName.C_Str())));
	}
	modelCreateDesc.Meshes = meshes;
	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		ProcessModelNode(node->mChildren[i], scene, modelCreateDesc);
	}
}

void ResourceFilter::ProcessMeshVertex(const aiMesh* mesh, const aiScene* scene, tagMeshCreateInfo& meshCreateInfo)
{
	uint32 n = mesh->mNumVertices;
	vector<Vertex> vertices(n);

	vec3 minBounds(FLT_MAX);
	vec3 maxBounds(-FLT_MAX);

	for(uint32 i = 0; i < n; ++i)
	{
		Vertex vertex;
		vertex.position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		if (mesh->HasNormals())
		{
			vertex.normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
		if (mesh->mTextureCoords[0])
		{
			vertex.texCoord = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}
		else
		{
			vertex.texCoord = vec2(0.0f, 0.0f);
		}

		if (mesh->HasTangentsAndBitangents())
		{
			vertex.tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
			vertex.bitangent = vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
		}
		minBounds = glm::min(minBounds, vertex.position);
		maxBounds = glm::max(maxBounds, vertex.position);
		vertices.push_back(vertex);
	}

	meshCreateInfo.VertexData = vertices.data();
	meshCreateInfo.VertexCount = n;
	meshCreateInfo.VertexStride = sizeof(Vertex);
	meshCreateInfo.BoundingBoxMin = minBounds;
	meshCreateInfo.BoundingBoxMax = maxBounds;
}

void ResourceFilter::ProcessMeshSkinData(const aiMesh* mesh, const aiScene* scene, tagMeshCreateInfo& meshCreateInfo)
{
	vector<VertexSkinData> skinData(mesh->mNumVertices);
	for (uint32 i = 0; i < mesh->mNumBones; ++i)
	{
		aiBone* bone = mesh->mBones[i];
		uint32 boneID = i;
		for (uint32 weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
		{
			aiVertexWeight weight = bone->mWeights[weightIndex];
			uint32 vertexID = weight.mVertexId;
			f32 weightValue = weight.mWeight;
			// Store the bone ID and weight for the vertex
			for (uint32 slot = 0; slot < 4; ++slot)
			{
				if (skinData[vertexID].weights[slot] == 0.0f)
				{
					skinData[vertexID].boneIDs[slot] = boneID;
					skinData[vertexID].weights[slot] = weightValue;
					break;
				}
			}
		}
		fmt::print("Bone Name: {}\n", bone->mName.C_Str());
	}
	meshCreateInfo.SkinData = skinData.data();
	meshCreateInfo.SkinDataCount = static_cast<uint32>(skinData.size());
	meshCreateInfo.SkinDataStride = sizeof(VertexSkinData);
}

void ResourceFilter::ProcessMeshIndex(const aiMesh* mesh, const aiScene* scene, tagMeshCreateInfo& meshCreateInfo)
{
	vector<uint32> indices;
	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		for (uint32 j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	meshCreateInfo.IndexData = indices.data();
	meshCreateInfo.IndexCount = static_cast<uint32>(indices.size());
	meshCreateInfo.IndexStride = sizeof(uint32);
}

void ResourceFilter::ProcessAnimations(const aiScene* scene)
{
	
}

void ResourceFilter::ProcessMaterial(const aiMaterial* material)
{
	aiColor4D color;
	aiString name;
	aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color);
}
