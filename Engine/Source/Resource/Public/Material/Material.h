#pragma once

#include "MaterialInterface.h"

struct tagMaterialDesc
{
	wstring PipelineKey = {};
};

BEGIN(Engine)

CLASS()
class ENGINE_API Material : public MaterialInterface
{
	REFLECT_CLASS(Material)
	using DESC = tagMaterialDesc;
#pragma region Constructor&Destructor
private:
	Material() {}
	virtual ~Material() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Material* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	virtual EResult Bind(uint32 slot) override;
#pragma endregion


#pragma region Variable
private:
#pragma endregion
};
END