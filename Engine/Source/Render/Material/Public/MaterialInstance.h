#pragma once

#include "MaterialInterface.h"
#include "Material.h"

struct MaterialInstanceDesc : public ResourceCreateDesc
{
	ResourceHandle<Material> BaseMaterialHandle;
};

BEGIN(Engine)

ENUM()
enum class EPipelineOverrideFlag : uint8
{
	None = 0,
	BlendMode = 1 << 0,
	CullMode = 1 << 1,
	FillMode = 1 << 2,
	DepthMode = 1 << 3,
	DepthCompareOp = 1 << 4,
	FrontFace = 1 << 5,
	PrimaryPassType = 1 << 6,
	Count
};

ENABLE_BITMASK_OPERATORS(EPipelineOverrideFlag)

CLASS()
class ENGINE_API MaterialInstance : public MaterialInterface
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(MaterialInstance)
	using DESC = MaterialInstanceDesc;
#pragma region Constructor&Destructor
private:
	MaterialInstance() : MaterialInterface(EResourceType::MaterialInstance) {}
	public: virtual ~MaterialInstance() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static MaterialInstance* Create(void* arg = nullptr);
	static MaterialInstance* Create(const ResourceHandle<Material>& baseMaterial);
	virtual MaterialInstance* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
	virtual void BuildMaterialConstants(MaterialConstants& outConstants) const override;
	virtual f32 GetAlphaCutoff() const override;
	virtual void BuildConstantBufferBlob(vector<uint8>& outBlob) const override;
	virtual const vector<uint8>& GetOrBuildConstantBufferBlob(uint32 liveTextureFlagMask) const override;
	virtual EResult Bind(uint32 slot) override;
#pragma endregion

#pragma region Base Material Management
public:
	void SetBaseMaterial(const ResourceHandle<Material>& material);
	Material* GetBaseMaterial() const { return m_BaseMaterialHandle.Get(); }
	const ResourceHandle<Material>& GetBaseMaterialHandle() const { return m_BaseMaterialHandle; }
#pragma endregion


#pragma region Pipeline
public:
	virtual ERenderPassType GetPrimaryPassType() const override;
	virtual EMaterialDomain GetDomain() const override;
	virtual EBlendMode GetBlendMode() const override;
	virtual ECullMode GetCullMode() const override;
	virtual EFillMode GetFillMode() const override;
	virtual EDepthMode GetDepthMode() const override;
	virtual ECompareOp GetDepthCompareOp() const override;
	virtual EFrontFace GetFrontFace() const override;
public:
	virtual void SetPrimaryPassType(ERenderPassType passType) override;
	virtual void SetBlendMode(EBlendMode mode) override;
	virtual void SetCullMode(ECullMode mode) override;
	virtual void SetFillMode(EFillMode mode) override;
	virtual void SetDepthMode(EDepthMode mode) override;
	virtual void SetDepthCompareOp(ECompareOp op) override;
	virtual void SetFrontFace(EFrontFace mode) override;
public:
	void ClearPrimaryPassTypeOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::PrimaryPassType); }
	void ClearBlendModeOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::BlendMode); MarkConstantBlobDirty(); }
	void ClearCullModeOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::CullMode); }
	void ClearFillModeOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::FillMode); }
	void ClearDepthModeOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthMode); }
	void ClearDepthCompareOpOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthCompareOp); }
	void ClearFrontFaceOverride() { RemoveFlag(m_OverrideFlags, EPipelineOverrideFlag::FrontFace); }
public:
	EPipelineOverrideFlag GetPipelineOverrideFlags() const { return m_OverrideFlags; }
	void SetPipelineOverrideFlags(EPipelineOverrideFlag flags) { m_OverrideFlags = flags; MarkConstantBlobDirty(); }
#pragma endregion


#pragma region Shader
public:
	virtual Shader* GetVertexShader(ERenderPassType passType = ERenderPassType::Geometry) const override;
	virtual Shader* GetPixelShader(ERenderPassType passType = ERenderPassType::Geometry) const override;
	virtual string GetShaderGraphPath() const override;
public:
	virtual void SetVertexShaderHandle(const ResourceHandle<Shader>& shader) override;
	virtual void SetPixelShaderHandle(const ResourceHandle<Shader>& shader) override;
#pragma endregion

#pragma region Variable
private:
	PROPERTY()
	ResourceHandle<Material> m_BaseMaterialHandle;
private:
	PROPERTY()
	EPipelineOverrideFlag m_OverrideFlags = EPipelineOverrideFlag::None;
	mutable const Material* m_CachedBaseMaterialPtr = nullptr;
	mutable uint64 m_CachedBaseConstantVersion = 0;
#pragma endregion
};
END
