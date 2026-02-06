#pragma once

#include "RenderComponent.h"


BEGIN(Engine)

CLASS()
class ENGINE_API MeshRenderer final : public RenderComponent
{
	REFLECT_CLASS(MeshRenderer);
#pragma region Constructor&Destructor
private:
	MeshRenderer() : RenderComponent{} {}
	virtual ~MeshRenderer() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Component* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Render
public:
	virtual EResult	Render(f32 dt)		override;
#pragma endregion

#pragma region Setter
public:
	void SetMesh(class Mesh* mesh);
#pragma endregion

#pragma region Variable
private:

	PROPERTY(NAME(L"Mesh"))
	class Mesh* m_Mesh = { nullptr };
#pragma endregion
};
END