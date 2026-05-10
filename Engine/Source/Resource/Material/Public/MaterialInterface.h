#pragma once

#include "Resource.h"
#include "Shader.h"
#include "Texture.h"
#include "RHIDefinitions.h"
#include "ResourceHandle.h"
#include "RHISampler.h"

BEGIN(Engine)

STRUCT()
struct MaterialTextureBinding
{
	REFLECT_STRUCT()

	PROPERTY(EDITABLE)
	uint32 slot = 0;

	PROPERTY(EDITABLE)
	string name = {};

	PROPERTY(EDITABLE)
	ResourceHandle<Texture> texture;

	PROPERTY(EDITABLE)
	bool hasCustomSampler = false;

	PROPERTY(EDITABLE)
	tagSamplerDesc samplerDesc = {};

	bool operator==(const MaterialTextureBinding& other) const = default;
};

ENUM()
enum class EMaterialParameterType
{
	Float,
	Float2,
	Float3,
	Float4,
	Int,
	Bool,
	Matrix,
};

STRUCT()
struct MaterialParameter
{
	REFLECT_STRUCT()

	PROPERTY()
	EMaterialParameterType type = EMaterialParameterType::Float;

	PROPERTY()
	vector<uint8> data;

	MaterialParameter() = default;
	MaterialParameter(EMaterialParameterType t, const void* ptr, size_t size)
		: type(t), data(size)
	{
		memcpy(data.data(), ptr, size);
	}

	bool operator==(const MaterialParameter& other) const = default;
};


struct tagMaterialDesc : public tagResourceCreateDesc
{
	ResourceHandle<Shader> VertexShaderHandle;
	ResourceHandle<Shader> PixelShaderHandle;
	EBlendMode BlendMode = EBlendMode::Opaque;
	ECullMode CullMode = ECullMode::Back;
	EFillMode FillMode = EFillMode::Solid;
	EFrontFace FrontFace = EFrontFace::Clockwise;
	EDepthMode DepthMode = EDepthMode::None;
	ECompareOp DepthCompareOp = ECompareOp::Less;
};

CLASS()
class ENGINE_API MaterialInterface : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(MaterialInterface)
#pragma region Constructor&Destructor
protected:
	using DESC = tagMaterialDesc;
	MaterialInterface() : Resource(EResourceType::Material) {}
	MaterialInterface(EResourceType type) : Resource(type) {}
	virtual ~MaterialInterface() = default;
public:
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	virtual EResult Bind(uint32 slot) override;
#pragma endregion

#pragma region Parameter Interface
public:
	const unordered_map<string, MaterialParameter>& GetParameters() const { return m_Parameters; }
	bool HasParameter(const string& name) const { return m_Parameters.find(name) != m_Parameters.end(); }
	bool RemoveParameter(const string& name) { return m_Parameters.erase(name) > 0; }
	void ClearParameters() { m_Parameters.clear(); }
	// Scalar
	void SetFloat(const string& name, f32 value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &value, sizeof(value)); }
	void SetFloat2(const string& name, const vec2& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float2, &value, sizeof(value)); }
	void SetFloat3(const string& name, const vec3& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float3, &value, sizeof(value)); }
	void SetFloat4(const string& name, const vec4& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &value, sizeof(value)); }
	void SetInt(const string& name, int32 value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Int, &value, sizeof(value)); }
	void SetBool(const string& name, bool value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Bool, &value, sizeof(value)); }
	void SetMatrix(const string& name, const mat4& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Matrix, &value, sizeof(value)); };

	// Getter
	template<typename T>
	T GetParameter(const string& name) const;	
#pragma endregion


#pragma region Texture Binding Interface
public:
	void SetTexture(const string& name, const ResourceHandle<Texture>& texture);
	void SetTextureBySlot(uint32 slot, const ResourceHandle<Texture>& texture);		
	void SetTextureBinding(const string& name, uint32 slot, const ResourceHandle<Texture>& texture);

	void SetSamplerDesc(const string& name, const tagSamplerDesc& desc);
	void SetSamplerDescBySlot(uint32 slot, const tagSamplerDesc& desc);

	Texture* GetTexture(const string& name) const;
	Texture* GetTextureBySlot(uint32 slot) const;
	ResourceHandle<Texture> GetTextureHandle(const string& name) const;
	ResourceHandle<Texture> GetTextureHandleBySlot(uint32 slot) const;

	const tagSamplerDesc* GetSamplerDesc(const string& name) const;
	const tagSamplerDesc* GetSamplerDescBySlot(uint32 slot) const;

	bool HasTextureBinding(const string& name) const;
	bool HasTextureBindingBySlot(uint32 slot) const;

	void RemoveTextureBinding(const string& name);
	void RemoveTextureBindingBySlot(uint32 slot);
	void ClearTextureBindings();

	const vector<MaterialTextureBinding>& GetTextureBindings() const { return m_TextureBindings; }
#pragma endregion


#pragma region Pipeline
public:
	virtual tagBlendState GetBlendState();
	virtual EBlendMode GetBlendMode() const { return m_BlendMode; }
	virtual ECullMode GetCullMode() const { return m_CullMode; }
	virtual EFrontFace GetFrontFace() const { return m_FrontFace; }
	virtual EFillMode GetFillMode() const { return m_FillMode; }
	virtual EDepthMode GetDepthMode() const { return m_DepthMode; }
	virtual ECompareOp GetDepthCompareOp() const { return m_DepthCompareOp; }
public:
	virtual void SetBlendMode(EBlendMode mode) { m_BlendMode = mode; }
	virtual void SetCullMode(ECullMode mode) { m_CullMode = mode; }
	virtual void SetFrontFace(EFrontFace mode) { m_FrontFace = mode; }
	virtual void SetFillMode(EFillMode mode) { m_FillMode = mode; }
	virtual void SetDepthMode(EDepthMode mode) { m_DepthMode = mode; }
	virtual void SetDepthCompareOp(ECompareOp op) { m_DepthCompareOp = op; }
#pragma endregion

#pragma region Shader
public:
	virtual Shader* GetVertexShader() const { return m_VertexShaderHandle.Get(); }
	virtual Shader* GetPixelShader() const { return m_PixelShaderHandle.Get(); }
public:
	virtual void SetVertexShaderHandle(const ResourceHandle<Shader>& shader) { m_VertexShaderHandle = shader; }
	virtual void SetPixelShaderHandle(const ResourceHandle<Shader>& shader) { m_PixelShaderHandle = shader; }
#pragma endregion


#pragma region Save&Load
public:
	virtual void Deserialize(Archive& ar) override;

protected:
	void RebuildTextureBindingCache();
	int32 FindTextureBindingIndexByName(const string& name) const;
	int32 FindTextureBindingIndexBySlot(uint32 slot) const;
#pragma endregion


#pragma region Variable
protected:
	PROPERTY(EDITABLE, CATEGORY(L"Pipeline"))
	EBlendMode m_BlendMode = { EBlendMode::Opaque };
	PROPERTY(EDITABLE, CATEGORY(L"Pipeline"))
	ECullMode m_CullMode = { ECullMode::Back };
	PROPERTY(EDITABLE, CATEGORY(L"Pipeline"))
	EFrontFace m_FrontFace = { EFrontFace::Clockwise };
	PROPERTY(EDITABLE, CATEGORY(L"Pipeline"))
	EFillMode m_FillMode = { EFillMode::Solid };
	PROPERTY(EDITABLE, CATEGORY(L"Pipeline"))
	EDepthMode m_DepthMode = { EDepthMode::None };
	PROPERTY(EDITABLE, CATEGORY(L"Pipeline"))
	ECompareOp m_DepthCompareOp = { ECompareOp::Less };

	PROPERTY(EDITABLE, CATEGORY(L"Shader"))
	ResourceHandle<Shader> m_VertexShaderHandle;

	PROPERTY(EDITABLE, CATEGORY(L"Shader"))
	ResourceHandle<Shader> m_PixelShaderHandle;

	PROPERTY(EDITABLE, CATEGORY(L"Parameter"))
	unordered_map<string, MaterialParameter> m_Parameters = {};

	PROPERTY(EDITABLE, CATEGORY("Texture"))
	vector<MaterialTextureBinding> m_TextureBindings = {};

private:
	unordered_map<string, uint32> m_TextureNameToIndex = {};
	unordered_map<uint32, uint32> m_TextureSlotToIndex = {};
#pragma endregion
};
END