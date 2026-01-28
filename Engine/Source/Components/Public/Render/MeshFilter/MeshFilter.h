#pragma once

#include "Component.h"

BEGIN(Engine)
class ENGINE_API MeshFilter : public Component
{
#pragma region Constructor&Destructor
private:
	MeshFilter();
	virtual ~MeshFilter() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static MeshFilter* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Variable
private:

#pragma endregion


};
END