#pragma once

#include "Component.h"
#include "Mesh.h"

BEGIN(Engine)
CLASS()
class ENGINE_API MeshFilter : public Component
{
	REFLECT_CLASS(MeshFilter)
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
	Mesh* GetMesh() { return m_Mesh; }
	void SetMesh(Mesh* mesh);
#pragma endregion


#pragma region Variable
private:
	PROPERTY()
	Mesh* m_Mesh = { nullptr };
#pragma endregion


};
END