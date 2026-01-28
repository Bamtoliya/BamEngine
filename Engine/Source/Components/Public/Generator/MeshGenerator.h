#pragma once

#include "Component.h"


#pragma region Struct
struct tagMeshGeneratorDesc : public tagComponentDesc
{

};
#pragma endregion


BEGIN(Engine)

class ENGINE_API MeshGenerator : public Component
{
	using DESC = tagMeshGeneratorDesc;
#pragma region Constructor&Destructor
protected:
	MeshGenerator() {}
	virtual ~MeshGenerator() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	virtual void Free() override;
#pragma endregion

#pragma region Management
public:
	virtual void Rebuild() PURE;
protected:
	void SetMesh(class Mesh* mesh);
#pragma endregion


#pragma region Variable
protected:
	class MeshRenderer* m_MeshRenderer = { nullptr };
	class Mesh* m_GeneratedMesh = { nullptr };
	bool m_Dirty = { false };
#pragma endregion



};
END