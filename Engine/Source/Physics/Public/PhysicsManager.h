#pragma once

#include "Base.h"
#include "RigidBody.h"

BEGIN(Engine)
class ENGINE_API PhysicsManager : public Base
{
	DECLARE_SINGLETON(PhysicsManager)
#pragma region Constructor&Destructor
private:
	PhysicsManager() {}
	virtual ~PhysicsManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region MyRegion

#pragma endregion


}
END