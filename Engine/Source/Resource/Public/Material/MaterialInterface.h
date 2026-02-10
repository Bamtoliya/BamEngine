#pragma once

#include "Resource.h"
#include "RHITexture.h"
#include "ReflectionMacro.h"

BEGIN(Engine)

struct TextureSlot
{
	uint32 slot = 0;
	RHITexture* texture = { nullptr };
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

CLASS()
class ENGINE_API MaterialInterface : public Resource
{
	REFLECT_CLASS(MaterialInterface)
#pragma region Constructor&Destructor
protected:
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
	virtual const wstring& GetPipelineKey() const { return m_PipelineKey; }
	void SetPipelineKey(const wstring& key) { m_PipelineKey = key; }
#pragma endregion

#pragma region Variable
protected:
	PROPERTY()
	wstring m_PipelineKey = {};

	PROPERTY()
	unordered_map<string, MaterialParameter> m_Parameters = {};

	PROPERTY()
	unordered_map<string, TextureSlot> m_TextureSlots = {};
#pragma endregion
};
END