#pragma once

#include "Resource.h"
#include "Shader.h"
#include "RHITexture.h"
#include "RHIDefinitions.h"

BEGIN(Engine)


STRUCT()
struct TextureSlot
{
	REFLECT_STRUCT(TextureSlot)
	PROPERTY()
	uint32 slot = 0;
	PROPERTY()
	RHITexture* texture = { nullptr };
	PROPERTY()
	wstring SamplerKey = {};
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
	REFLECT_STRUCT(MaterialParameter)

	PROPERTY()
	EMaterialParameterType type;

	PROPERTY()
	vector<uint8> data;

	MaterialParameter() = default;
	MaterialParameter(EMaterialParameterType t, const void* ptr, size_t size)
		: type(t), data(size)
	{
		memcpy(data.data(), ptr, size);
	}
};


struct tagMaterialDesc
{
	Shader* VertexShader = { nullptr };
	Shader* PixelShader = { nullptr };
	EBlendMode BlendMode = EBlendMode::Opaque;
	ECullMode CullMode = ECullMode::Back;
	EFillMode FillMode = EFillMode::Solid;
	EDepthMode DepthMode = EDepthMode::None;
	ECompareOp DepthCompareOp = ECompareOp::Less;
};

CLASS()
class ENGINE_API MaterialInterface : public Resource
{
	REFLECT_CLASS(MaterialInterface)
#pragma region Constructor&Destructor
protected:
	using DESC = tagMaterialDesc;
	MaterialInterface() {}
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
	// Scalar
	void SetFloat(const string& name, f32 value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &value, sizeof(value)); }
	void SetFloat2(const string& name, const vec2& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float2, &value, sizeof(value)); }
	void SetFloat3(const string& name, const vec3& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float3, &value, sizeof(value)); }
	void SetFloat4(const string& name, const vec4& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &value, sizeof(value)); }
	void SetInt(const string& name, int32 value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Int, &value, sizeof(value)); }
	void SetBool(const string& name, bool value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Bool, &value, sizeof(value)); }
	void SetMatrix(const string& name, const mat4& value) { m_Parameters[name] = MaterialParameter(EMaterialParameterType::Matrix, &value, sizeof(value)); };

	// Texture/Sampler
	void SetTexture(const string& name, RHITexture* texture);
	void SetTextureBySlot(uint32 slot, RHITexture* texture);
	void SetSampler(const string& name, RHISampler* sampler);

	// Getter
	template<typename T>
	T GetParameter(const string& name) const;
	RHITexture* GetTexture(const string& name) const;
	RHITexture* GetTextureBySlot(uint32 slot) const;
	RHISampler* GetSampler(const string& name) const;
	unordered_map<string, TextureSlot>& GetTextureSlots() { return m_TextureSlots; }
#pragma endregion

#pragma region Pipeline
public:
	virtual EBlendMode GetBlendMode() const { return m_BlendMode; }
	virtual ECullMode GetCullMode() const { return m_CullMode; }
	virtual EFillMode GetFillMode() const { return m_FillMode; }
	virtual EDepthMode GetDepthMode() const { return m_DepthMode; }
	virtual ECompareOp GetDepthCompareOp() const { return m_DepthCompareOp; }
public:
	void SetBlendMode(EBlendMode mode) { m_BlendMode = mode; }
	void SetCullMode(ECullMode mode) { m_CullMode = mode; }
	void SetFillMode(EFillMode mode) { m_FillMode = mode; }
	void SetDepthMode(EDepthMode mode) { m_DepthMode = mode; }
	void SetDepthCompareOp(ECompareOp op) { m_DepthCompareOp = op; }
#pragma endregion

#pragma region Shader
public:
	virtual Shader* GetVertexShader() const { return m_VertexShader; }
	virtual Shader* GetPixelShader() const { return m_PixelShader; }
public:
	virtual void SetVertexShader(Shader* shader) { m_VertexShader = shader; }
	virtual void SetPixelShader(Shader* shader) { m_PixelShader = shader; }
#pragma endregion

#pragma region Variable
protected:
	PROPERTY(CATEGORY(L"Pipeline"))
	EBlendMode m_BlendMode = { EBlendMode::Opaque };
	PROPERTY(CATEGORY(L"Pipeline"))
	ECullMode m_CullMode = { ECullMode::Back };
	PROPERTY(CATEGORY(L"Pipeline"))
	EFillMode m_FillMode = { EFillMode::Solid };
	PROPERTY(CATEGORY(L"Pipeline"))
	EDepthMode m_DepthMode = { EDepthMode::None };
	PROPERTY(CATEGORY(L"Pipeline"))
	ECompareOp m_DepthCompareOp = { ECompareOp::Less };

	PROPERTY()
	Shader* m_VertexShader = { nullptr };

	PROPERTY()
	Shader* m_PixelShader = { nullptr };

	PROPERTY()
	unordered_map<string, MaterialParameter> m_Parameters = {};

	PROPERTY()
	unordered_map<string, TextureSlot> m_TextureSlots = {};
#pragma endregion
};
END