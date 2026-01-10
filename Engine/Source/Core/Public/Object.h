#pragma once
#include "BamEngine/Base.h"

class ENGINE_API Object : public Base
{
#pragma region Constructor&Destructor
private:
	Object() {}
	virtual ~Object() = default;
	virtual void Free() override {};
#pragma endregion

#pragma region Variables
private:

#pragma endregion



};