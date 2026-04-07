#pragma once

#include "Component.h"
#include "Mesh.h"
#include "ResourceHandle.h"

BEGIN(Engine)
CLASS()
class ENGINE_API MeshFilter : public Component
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
private:
	MeshFilter() {}
	virtual ~MeshFilter() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static MeshFilter* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion


#pragma region Management
public:
	Mesh* GetMesh() { if(!m_MeshHandle.IsValid()) return nullptr; return m_MeshHandle.Get(); }
	const ResourceHandle<Mesh>& GetMeshHandle() const { return m_MeshHandle; }
	void SetMeshHandle(ResourceHandle<Mesh> handle);
#pragma endregion


#pragma region Variable
private:
	PROPERTY()
	ResourceHandle<Mesh> m_MeshHandle;
#pragma endregion
};
END