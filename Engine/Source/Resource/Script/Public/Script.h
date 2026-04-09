#pragma once

#include "Resource.h"

struct tagScripteDesc : public tagResourceCreateDesc
{
};

BEGIN(Engine)

CLASS()
class Script : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Script)
	using DESC = tagScripteDesc;

#pragma region Constructor&Destructor
private:
	Script() : Resource(EResourceType::Script) {}
	virtual ~Script() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Script* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Member Variables
private:
#pragma endregion
};
END