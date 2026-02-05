#pragma once

#include "Base.h"
#include "Editor_Includes.h"

BEGIN(Editor)
class QtManager : public Base
{
	DECLARE_SINGLETON(QtManager)

#pragma region Constructor&Destructor
private:
	QtManager() {}
	virtual ~QtManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

};
END