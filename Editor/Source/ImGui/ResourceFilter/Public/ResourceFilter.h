#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"

#undef DBG_NEW
#undef new
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#ifndef DBG_NEW 
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
#define new DBG_NEW 
#endif

BEGIN(Editor)
class ResourceFilter : public ImGuiInterface
{
public:
	ResourceFilter() = default;
	virtual ~ResourceFilter() = default;
	virtual void Draw() override;

private:
	void ImportModel(wstring filepath);
	void ProcessModelNode(const aiNode* node, const aiScene* scene, tagModelCreateDesc& modelCreateDesc);
	void ProcessMeshVertex(const aiMesh* mesh, const aiScene* scene, tagMeshCreateInfo& meshCreateInfo);
	void ProcessMeshSkinData(const aiMesh* mesh, const aiScene* scene, tagMeshCreateInfo& meshCreateInfo);
	void ProcessMeshIndex(const aiMesh* mesh, const aiScene* scene, tagMeshCreateInfo& meshCreateInfo);

	void ProcessAnimations(const aiScene* scene);
	void ProcessMaterial(const aiMaterial* material);


	
#pragma region Member Variables
private:
	wstring m_Filter;
#pragma endregion
};
END