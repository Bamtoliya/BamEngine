#pragma once

#include "Component.h"

BEGIN(Engine)
class ENGINE_API MeshRenderer final : public Component
{
#pragma region Constructor&Destructor
private:
	MeshRenderer() : Component{} {}
	virtual ~MeshRenderer() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Component* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void	Update(f32 dt) override;
	virtual EResult	Render(f32 dt) override;
#pragma endregion

#pragma region Setter
public:
	void SetMesh(class Mesh* mesh);
#pragma endregion


#pragma region Variable
private:
	class Mesh* m_Mesh = { nullptr };
#pragma endregion
};
END