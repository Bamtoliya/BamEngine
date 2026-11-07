#pragma once

#include "Resource.h"
#include "ResourceHandle.h"

#include "Mesh.h"
#include "Material.h"
#include "Skeleton.h"
#include "Animation.h"

BEGIN(Engine)

STRUCT()
struct ENGINE_API ModelNode
{
	REFLECT_BODY()

	PROPERTY()
	wstring Name;

	PROPERTY()
	int32 ParentIndex = -1;

	PROPERTY()
	mat4 LocalTransform = mat4(1.f);

	PROPERTY()
	vector<uint32> MeshIndices;

	PROPERTY(ALLOW_MISSING)
	wstring SourceNodeKey;

	PROPERTY(ALLOW_MISSING)
	vector<wstring> MeshAttachmentKeys;
};

STRUCT()
struct ENGINE_API ModelMeshLOD
{
	REFLECT_BODY()

	PROPERTY(ALLOW_MISSING)
	ResourceHandle<Mesh> LOD1Mesh;

	PROPERTY(ALLOW_MISSING)
	ResourceHandle<Mesh> LOD2Mesh;
};

struct ModelCreateDesc : public ResourceCreateDesc
{
	vector<ModelNode> Nodes;
	vector<ResourceHandle<Mesh>> Meshes;
	vector<ResourceHandle<MaterialInterface>> Materials;
	ResourceHandle<Skeleton> Skeleton;
	vector<ResourceHandle<Animation>> Animations;
	vector<uint32> MeshMaterialIndices;
	vector<ModelMeshLOD> MeshLODs;
};

CLASS()
class ENGINE_API Model : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Model)
	using DESC = ModelCreateDesc;
	friend class ResourceManager;
#pragma region Constrcutor&Destructor
private:
	Model() : Resource(EResourceType::Model) {}
	virtual ~Model() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static Model* Create(void* arg = nullptr);
	virtual void Free() override;
protected:
	EResult OnPostLoad() override;
#pragma endregion

#pragma region Bind
public:
	EResult Bind(uint32 slot) override;
#pragma endregion

#pragma region Getter
public:
	vector<ResourceHandle<Mesh>> GetMeshes() const { return m_Meshes; }
	vector<ResourceHandle<MaterialInterface>> GetMaterials() const { return m_Materials; }
	ResourceHandle<Skeleton> GetSkeleton() const { return m_Skeleton; }
	vector<ResourceHandle<Animation>> GetAnimations() const { return m_Animations; }
	const vector<ModelNode>& GetNodes() const { return m_Nodes; }
	const vector<uint32>& GetMeshMaterialIndices() const { return m_MeshMaterialIndices; }
	ResourceHandle<MaterialInterface> GetMeshMaterial(uint32 meshIndex) const;
	const vector<ModelMeshLOD>& GetMeshLODs() const { return m_MeshLODs; }
	ResourceHandle<Mesh> GetMeshLOD(uint32 meshIndex, uint32 lodLevel) const
	{
		if (lodLevel == 0 && meshIndex < m_Meshes.size()) return m_Meshes[meshIndex];
		if (meshIndex >= m_MeshLODs.size()) return {};
		if (lodLevel == 1) return m_MeshLODs[meshIndex].LOD1Mesh;
		if (lodLevel == 2) return m_MeshLODs[meshIndex].LOD2Mesh;
		return {};
	}
	bool HasLODs() const
	{
		for (const auto& lod : m_MeshLODs)
		{
			if (lod.LOD1Mesh.IsValid() || lod.LOD2Mesh.IsValid()) return true;
		}
		return false;
	}
#pragma endregion

#pragma region Setter
public:
	EResult SetMaterial(uint32 slotIndex, const ResourceHandle<MaterialInterface>& material);
#pragma endregion


#pragma region Save&Load
public:
	[[nodiscard]] archive::ArchiveResult ReadArchive(const archive::IArchive& ar) override;
#pragma endregion

#pragma region Member Variable
private:
	PROPERTY()
	vector<ModelNode> m_Nodes;
	PROPERTY()
	vector<ResourceHandle<Mesh>> m_Meshes;
	PROPERTY()
	vector<ResourceHandle<MaterialInterface>> m_Materials;
	PROPERTY()
	ResourceHandle<Skeleton> m_Skeleton;
	PROPERTY()
	vector<ResourceHandle<Animation>> m_Animations;
	PROPERTY()
	vector<uint32> m_MeshMaterialIndices;
	PROPERTY(ALLOW_MISSING)
	vector<ModelMeshLOD> m_MeshLODs;
#pragma endregion
};
END