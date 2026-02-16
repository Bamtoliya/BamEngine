#pragma once

#include "Collider.h"

BEGIN(Engine)
class ENGINE_API BoxCollider : public Collider
{
	REFLECT_CLASS(BoxCollider)
#pragma region Constructor&Destructor
private:
	BoxCollider() {}
	virtual ~BoxCollider() {}
	EResult Initialize(void* arg = nullptr);
public:
	static BoxCollider* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion


#pragma region Member Variable

#pragma endregion

}
END