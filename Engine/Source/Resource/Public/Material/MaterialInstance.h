#pragma once

#include "MaterialInterface.h"

struct tagMaterialInstanceDesc
{
	class Material* BaseMaterial = { nullptr };
};

BEGIN(Engine)

CLASS()
class ENGINE_API MaterialInstance : public MaterialInterface
{
	REFLECT_CLASS(MaterialInstance)
	using DESC = tagMaterialInstanceDesc;
#pragma region Constructor&Destructor
private:
	MaterialInstance() {}
	virtual ~MaterialInstance() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static MaterialInstance* Create(void* arg = nullptr);
	static MaterialInstance* Create(Material* baseMaterial);
	virtual MaterialInstance* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
	virtual EResult Bind(uint32 slot) override;
	virtual const wstring& GetPipelineKey() const override;
#pragma endregion


#pragma region Base Material Management
	void SetBaseMaterial(Material* material);
	Material* GetBaseMaterial() const { return m_BaseMaterial; }
#pragma endregion


#pragma region Variable
private:
	PROPERTY()
	class Material* m_BaseMaterial = { nullptr };
#pragma endregion
};
END