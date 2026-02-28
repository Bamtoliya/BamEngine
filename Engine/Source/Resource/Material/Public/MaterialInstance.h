#pragma once

#include "MaterialInterface.h"

struct tagMaterialInstanceDesc
{
	class Material* BaseMaterial = { nullptr };
};

ENUM()
enum class EPipelineOverrideFlag : uint8
{
	None = 0,
	BlendMode = 1 << 0,
	CullMode = 1 << 1,
	FillMode = 1 << 2,
	DepthMode = 1 << 3,
	DepthCompareOp = 1 << 4,
	Count
};

ENABLE_BITMASK_OPERATORS(EPipelineOverrideFlag)

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
#pragma endregion


#pragma region Base Material Management
public:
	void SetBaseMaterial(Material* material);
	Material* GetBaseMaterial() const { return m_BaseMaterial; }
#pragma endregion


#pragma region Pipeline
public:
	virtual EBlendMode GetBlendMode() const override;
	virtual ECullMode GetCullMode() const override;
	virtual EFillMode GetFillMode() const override;
	virtual EDepthMode GetDepthMode() const override;
	virtual ECompareOp GetDepthCompareOp() const override;
#pragma endregion


#pragma region Shader
public:
	virtual Shader* GetVertexShader() const;
	virtual Shader* GetPixelShader() const;
public:
	virtual void SetVertexShader(Shader* shader);
	virtual void SetPixelShader(Shader* shader);
#pragma endregion



#pragma region Variable
private:
	PROPERTY()
	class Material* m_BaseMaterial = { nullptr };
private:
	PROPERTY()
	EPipelineOverrideFlag m_OverrideFlags = EPipelineOverrideFlag::None;
#pragma endregion
};
END