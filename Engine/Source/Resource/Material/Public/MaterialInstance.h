#pragma once

#include "MaterialInterface.h"

struct tagMaterialInstanceDesc
{
	ResourceHandle<Material> BaseMaterialHandle;
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
	REFLECT_CLASS()
	using DESC = tagMaterialInstanceDesc;
#pragma region Constructor&Destructor
private:
	MaterialInstance() : MaterialInterface(EResourceType::MaterialInstance) {}
	virtual ~MaterialInstance() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static MaterialInstance* Create(void* arg = nullptr);
	static MaterialInstance* Create(const ResourceHandle<Material>& baseMaterial);
	virtual MaterialInstance* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
	virtual EResult Bind(uint32 slot) override;
#pragma endregion


#pragma region Base Material Management
public:
	void SetBaseMaterial(const ResourceHandle<Material>& material);
	Material* GetBaseMaterial() const { return m_BaseMaterialHandle.Get(); }
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
	virtual void SetVertexShaderHandle(const ResourceHandle<Shader>& shader);
	virtual void SetPixelShaderHandle(const ResourceHandle<Shader>& shader);
#pragma endregion

#pragma region Save&Load
public:
	virtual void Deserialize(Archive& ar);
#pragma endregion



#pragma region Variable
private:
	PROPERTY()
	ResourceHandle<Material> m_BaseMaterialHandle;
private:
	PROPERTY()
	EPipelineOverrideFlag m_OverrideFlags = EPipelineOverrideFlag::None;
#pragma endregion
};
END