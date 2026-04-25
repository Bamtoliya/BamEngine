#pragma once

#include "Resource.h"
#include "ResourceHandle.h"
#include "ResourceHandle.inl"

#include "Mesh.h"
#include "Material.h"
#include "Skeleton.h"
#include "Animation.h"

BEGIN(Engine)

struct tagModelCreateDesc
{
	vector<ResourceHandle<Mesh>> Meshes;
	vector<ResourceHandle<Material>> Materials;
	ResourceHandle<Skeleton> Skeleton;
	vector<ResourceHandle<Animation>> Animations;
};

CLASS()
class ENGINE_API Model : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Model)
	using DESC = tagModelCreateDesc;
#pragma region Constrcutor&Destructor
private:
	Model() : Resource(EResourceType::Model) {}
	virtual ~Model() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Model* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	EResult Bind(uint32 slot) override;
#pragma endregion

#pragma region Getter
public:
	vector<ResourceHandle<Mesh>> GetMeshes() const { return m_Meshes; }
	vector<ResourceHandle<Material>> GetMaterials() const { return m_Materials; }
	ResourceHandle<Skeleton> GetSkeleton() const { return m_Skeleton; }
	vector<ResourceHandle<Animation>> GetAnimations() const { return m_Animations; }
#pragma endregion

#pragma region Setter

#pragma endregion

#pragma region Member Variable
private:
	PROPERTY()
	vector<ResourceHandle<Mesh>> m_Meshes;

	PROPERTY()
	vector<ResourceHandle<Material>> m_Materials;

	PROPERTY()
	ResourceHandle<Skeleton> m_Skeleton;

	PROPERTY()
	vector<ResourceHandle<Animation>> m_Animations;
#pragma endregion
};
END