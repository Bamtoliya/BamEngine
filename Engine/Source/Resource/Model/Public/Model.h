#pragma once

#include "Resource.h"
#include "Material.h"
#include "Mesh.h"
#include "Bone.h"
#include "Skeleton.h"

struct tagModelCreateDesc
{
	vector<Mesh*> Meshes;
	vector<Material*> Materials;
	Skeleton* Skeleton = { nullptr };
};

BEGIN(Engine)

CLASS()
class ENGINE_API Model : public Resource
{
	REFLECT_CLASS()
	using DESC = tagModelCreateDesc;
#pragma region Constrcutor&Destructor
private:
	Model() {}
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
	vector<Material*> GetMaterials() const { return m_Materials; }
	vector<Mesh*> GetMeshes() const { return m_Meshes; }
	Skeleton* GetSkeleton() const { return m_Skeleton; }
#pragma endregion

#pragma region Setter

#pragma endregion

#pragma region Member Variable
private:
	PROPERTY()
	vector<Material*> m_Materials;

	PROPERTY()
	vector<Mesh*> m_Meshes;

	PROPERTY()
	Skeleton* m_Skeleton = { nullptr };
#pragma endregion
};
END