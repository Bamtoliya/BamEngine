#pragma once

#include "ImporterInterface.h"

#undef DBG_NEW
#undef new
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#ifndef DBG_NEW 
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
#define new DBG_NEW 
#endif


ENUM()
enum class EModelPostProcessFlag : uint32
{
	None = 0,
	CalcTangentSpace = aiProcess_CalcTangentSpace,
	JoinIdenticalVertices = aiProcess_JoinIdenticalVertices,
	MakeLeftHanded = aiProcess_MakeLeftHanded,
	Triangulate = aiProcess_Triangulate,
	RemoveComponent = aiProcess_RemoveComponent,
	GenNormals = aiProcess_GenNormals,
	GenSmoothNormals = aiProcess_GenSmoothNormals,
	SplitLargeMeshes = aiProcess_SplitLargeMeshes,
	PreTransformVertices = aiProcess_PreTransformVertices,
	LimitBoneWeights = aiProcess_LimitBoneWeights,
	ValidateDataStructure = aiProcess_ValidateDataStructure,
	ImproveCacheLocality = aiProcess_ImproveCacheLocality,
	RemoveRedundantMaterials = aiProcess_RemoveRedundantMaterials,
	FixInfacingNormals = aiProcess_FixInfacingNormals,
	SortByPType = aiProcess_SortByPType,
	FindDegenerates = aiProcess_FindDegenerates,
	FindInvalidData = aiProcess_FindInvalidData,
	GenUVCoords = aiProcess_GenUVCoords,
	TransformUVCoords = aiProcess_TransformUVCoords,
	FindInstances = aiProcess_FindInstances,
	OptimizeMeshes = aiProcess_OptimizeMeshes,
	OptimizeGraph = aiProcess_OptimizeGraph,
	FlipUVs = aiProcess_FlipUVs,
	FlipWindingOrder = aiProcess_FlipWindingOrder,
};

ENABLE_BITMASK_OPERATORS(EModelPostProcessFlag);

ENUM()
enum class EModelImportOption : uint32
{
	None = 0,
	Animations = 1 << 0,
	Materials = 1 << 1,
	Meshes = 1 << 2,
	Skeleton = 1 << 3,
};

ENABLE_BITMASK_OPERATORS(EModelImportOption);

struct tagModelImportDesc
{
	EModelPostProcessFlag PostProcessFlags = EModelPostProcessFlag::Triangulate | EModelPostProcessFlag::FlipUVs;
	EModelImportOption ImportOptions = EModelImportOption::Meshes | EModelImportOption::Materials;
};

BEGIN(Editor)
class ModelImporter : public ImporterInterface
{
#pragma region Contructor&Destructor
	using DESC = tagModelImportDesc;
private:
	ModelImporter() = default;
	virtual ~ModelImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static ModelImporter* Create() { return new ModelImporter(); }
	virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion
public:
	virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr) override;

private:
	void ProcessModelNode(
		const aiNode* node,
		const aiScene* scene,
		const filesystem::path& outputDir,
		const wstring& sourceStem,
		unordered_map<wstring, uint32>& nameCounts,
		uint32& meshSerial,
		tagModelCreateDesc& modelCreateDesc);
	void ProcessMeshVertex(
		const aiMesh* mesh,
		const aiScene* scene,
		tagMeshCreateDesc& meshCreateInfo,
		vector<Vertex>& outVertices);
	void ProcessMeshSkinData(
		const aiMesh* mesh,
		const aiScene* scene,
		tagMeshCreateDesc& meshCreateInfo,
		vector<VertexSkinData>& outSkinData);
	void ProcessMeshIndex(
		const aiMesh* mesh,
		const aiScene* scene,
		tagMeshCreateDesc& meshCreateInfo,
		vector<uint32>& outIndices);
	void ProcessSkeleton(
		const aiScene* scene,
		const filesystem::path& outputDir,
		const wstring& sourceStem,
		tagModelCreateDesc& modelCreateDesc);
	void ProcessAnimations(
		const aiScene* scene,
		const filesystem::path& outputDir,
		const wstring& sourceStem,
		tagModelCreateDesc& modelCreateDesc);
	void ProcessMaterial(
		const aiMaterial* aiMat,
		uint32 matIndex,
		const aiScene* scene,
		const filesystem::path& outputDir,
		const wstring& sourceStem,
		tagModelCreateDesc& modelCreateDesc);

#pragma region Member Variables
private:
#pragma endregion

};
END