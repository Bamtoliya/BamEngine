#pragma once

#include "Resource.h"
#include "Shader.h"
#include "Texture.h"
#include "RHITypes.h"
#include "ResourceHandle.h"
#include "RHISampler.h"
#include "RenderPassTypes.h"
#include <optional>

BEGIN(Engine)

STRUCT()
struct MaterialTextureBinding
{
	REFLECT_BODY()

	PROPERTY(EDITABLE)
	uint32 slot = 0;

	PROPERTY(EDITABLE)
	string name = {};

	PROPERTY(EDITABLE)
	ResourceHandle<Texture> texture;

	PROPERTY(EDITABLE)
	bool hasCustomSampler = false;

	PROPERTY(EDITABLE)
	SamplerDesc samplerDesc = {};

	bool operator==(const MaterialTextureBinding& other) const = default;
};

STRUCT()
struct MaterialPassShader
{
	REFLECT_BODY()
	PROPERTY(EDITABLE)
	ERenderPassType passType = ERenderPassType::Geometry;
	PROPERTY(EDITABLE)
	ResourceHandle<Shader> vertexShader;
	PROPERTY(EDITABLE)
	ResourceHandle<Shader> pixelShader;
	bool operator==(const MaterialPassShader& other) const = default;
};
struct MaterialPassShaderDesc
{
	ERenderPassType PassType = ERenderPassType::Geometry;
	ResourceHandle<Shader> VertexShaderHandle;
	ResourceHandle<Shader> PixelShaderHandle;
};

// 1. 머티리얼 비트 플래그 열거형 정의
ENUM()
enum class EMaterialFeatureFlags : uint32
{
	None = 0,
	HasDiffuse = 1 << 0, // 슬롯 0: Base Color / Albedo Map
	HasNormal = 1 << 1, // 슬롯 1: Normal Map
	HasPBR = 1 << 2, // 슬롯 2: PBR Map (Roughness/Metallic/Cavity/AO)
	HasEmissive = 1 << 3, // 슬롯 3: Emissive Map
};
ENABLE_BITMASK_OPERATORS(EMaterialFeatureFlags)
// 2. HLSL 16바이트 정렬을 준수하는 머티리얼 상수 버퍼 구조체 (총 48바이트)
struct MaterialConstants
{
	uint32 materialFlags = 0;
	float  roughness = 0.2f;
	float  metallic = 0.0f;
	float  cavity = 0.5f;
	vec4   baseColor = vec4(1.0f);
	vec3   emissiveColor = vec3(0.0f);
	float  ao = 1.0f;
};
static_assert(sizeof(MaterialConstants) == 48, "MaterialConstants must be 48 bytes (aligned to 16 bytes)");

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
	REFLECT_BODY()

	PROPERTY()
	EMaterialParameterType type = EMaterialParameterType::Float;

	PROPERTY()
	uint32 offset = 0;

	PROPERTY()
	vector<uint8> data;

	PROPERTY()
	vector<uint8> defaultValue;

	PROPERTY()
	f32 minValue = 0.0f;

	PROPERTY()
	f32 maxValue = 1.0f;

	PROPERTY()
	string displayNameKey;

	PROPERTY()
	bool isColor = false;

	MaterialParameter() = default;
	MaterialParameter(EMaterialParameterType t, const void* ptr, size_t size, uint32 inOffset = 0,
		f32 inMin = 0.0f, f32 inMax = 1.0f, const string& inKey = "", bool inIsColor = false)
		: type(t), offset(inOffset), data(size), defaultValue(size),
		minValue(inMin), maxValue(inMax), displayNameKey(inKey), isColor(inIsColor)
	{
		memcpy(data.data(), ptr, size);
		memcpy(defaultValue.data(), ptr, size);
	}

	bool operator==(const MaterialParameter& other) const = default;
};


struct MaterialDesc : public ResourceCreateDesc
{
	ResourceHandle<Shader> VertexShaderHandle;
	ResourceHandle<Shader> PixelShaderHandle;
	vector<MaterialPassShaderDesc> PassShaders = {};
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
	using DESC = MaterialDesc;
	MaterialInterface() : Resource(EResourceType::Material) {}
	MaterialInterface(EResourceType type) : Resource(type) {}
public:
	virtual ~MaterialInterface() = default;
	virtual void Free() override;
	virtual EResult Reload(const std::filesystem::path& filePath = {}) override;
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

	// 셰이더 레이아웃 순서대로 파라미터를 등록하는 자동 오프셋 누적 API (메타데이터 지원)
	void AddFloat(const string& name, f32 defaultValue = 0.0f, float min = 0.0f, float max = 1.0f, const string& localKey = "");
	void AddFloat2(const string& name, const vec2& defaultValue = vec2(0.0f), float min = 0.0f, float max = 1.0f, const string& localKey = "");
	void AddFloat3(const string& name, const vec3& defaultValue = vec3(0.0f), float min = 0.0f, float max = 1.0f, const string& localKey = "");
	void AddColor3(const string& name, const vec3& defaultColor = vec3(1.0f), const string& localKey = "");
	void AddFloat4(const string& name, const vec4& defaultValue = vec4(0.0f), float min = 0.0f, float max = 1.0f, const string& localKey = "");
	void AddColor4(const string& name, const vec4& defaultColor = vec4(1.0f), const string& localKey = "");
	void AddInt(const string& name, int32 defaultValue = 0, int32 min = 0, int32 max = 100, const string& localKey = "");
	void AddBool(const string& name, bool defaultValue = false, const string& localKey = "");

	// 파라미터 값 설정 (이미 등록된 파라미터면 기존 오프셋을 100% 보존하고 값만 갱신!)
	void SetFloat(const string& name, f32 value);
	void SetFloat2(const string& name, const vec2& value);
	void SetFloat3(const string& name, const vec3& value);
	void SetFloat4(const string& name, const vec4& value);
	void SetInt(const string& name, int32 value);
	void SetBool(const string& name, bool value);
	void SetMatrix(const string& name, const mat4& value);

public:
	virtual void BuildMaterialConstants(MaterialConstants& outConstants) const;

	template<typename T>
	T GetParameter(const string& name, const T& defaultValue = T{}) const
	{
		auto it = m_Parameters.find(name);
		if (it != m_Parameters.end() && it->second.data.size() >= sizeof(T))
		{
			return *reinterpret_cast<const T*>(it->second.data.data());
		}
		return defaultValue;
	}

	// 파라미터 초기값 복원(Reset) API
	void ResetParameter(const string& name);
	void ResetAllParameters();

private:
	// 다음 파라미터가 배치될 최적의 정렬 오프셋을 계산하는 내부 헬퍼
	uint32 CalculateNextOffset(size_t typeSize, size_t alignment) const;
#pragma endregion


#pragma region Texture Binding Interface
public:
	void SetTexture(const string& name, const ResourceHandle<Texture>& texture);
	void SetTextureBySlot(uint32 slot, const ResourceHandle<Texture>& texture);		
	void SetTextureBinding(const string& name, uint32 slot, const ResourceHandle<Texture>& texture);

	void SetSamplerDesc(const string& name, const SamplerDesc& desc);
	void SetSamplerDescBySlot(uint32 slot, const SamplerDesc& desc);

	Texture* GetTexture(const string& name) const;
	Texture* GetTextureBySlot(uint32 slot) const;
	ResourceHandle<Texture> GetTextureHandle(const string& name) const;
	ResourceHandle<Texture> GetTextureHandleBySlot(uint32 slot) const;

	const SamplerDesc* GetSamplerDesc(const string& name) const;
	const SamplerDesc* GetSamplerDescBySlot(uint32 slot) const;

	bool HasTextureBinding(const string& name) const;
	bool HasTextureBindingBySlot(uint32 slot) const;

	void RemoveTextureBinding(const string& name);
	void RemoveTextureBindingBySlot(uint32 slot);
	void ClearTextureBindings();

	const vector<MaterialTextureBinding>& GetTextureBindings() const { return m_TextureBindings; }
#pragma endregion


#pragma region Pipeline
public:
	virtual BlendState GetBlendState();
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
	virtual Shader* GetVertexShader(ERenderPassType passType = ERenderPassType::Geometry) const;
	virtual Shader* GetPixelShader(ERenderPassType passType = ERenderPassType::Geometry) const;
	ResourceHandle<Shader> GetVertexShaderHandle(ERenderPassType passType = ERenderPassType::Geometry) const { return m_VertexShaderHandle; };
	ResourceHandle<Shader> GetPixelShaderHandle(ERenderPassType passType = ERenderPassType::Geometry) const { return m_PixelShaderHandle; };
public:
	virtual void SetVertexShaderHandle(const ResourceHandle<Shader>& shader) { m_VertexShaderHandle = shader; }
	virtual void SetPixelShaderHandle(const ResourceHandle<Shader>& shader) { m_PixelShaderHandle = shader; }
	void SetPassShader(ERenderPassType passType, const ResourceHandle<Shader>& vs, const ResourceHandle<Shader>& ps = {});
	bool HasPassShader(ERenderPassType passType) const;
	void RemovePassShader(ERenderPassType passType);
	const vector<MaterialPassShader>& GetPassShaders() const { return m_PassShaders; }
#pragma endregion

#pragma region Save&Load
protected:
	EResult OnPostLoad() override;
	void RebuildTextureBindingCache();
	int32 FindTextureBindingIndexByName(const string& name) const;
	int32 FindTextureBindingIndexBySlot(uint32 slot) const;
#pragma endregion


#pragma region Variable
protected:
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PIPELINE"), DISPLAY_NAME("PROP_BLEND_MODE"))
	EBlendMode m_BlendMode = { EBlendMode::Opaque };
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PIPELINE"), DISPLAY_NAME("PROP_CULL_MODE"))
	ECullMode m_CullMode = { ECullMode::Back };
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PIPELINE"), DISPLAY_NAME("PROP_FRONT_FACE"))
	EFrontFace m_FrontFace = { EFrontFace::Clockwise };
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PIPELINE"), DISPLAY_NAME("PROP_FILL_MODE"))
	EFillMode m_FillMode = { EFillMode::Solid };
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PIPELINE"), DISPLAY_NAME("PROP_DEPTH_MODE"))
	EDepthMode m_DepthMode = { EDepthMode::None };
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PIPELINE"), DISPLAY_NAME("PROP_DEPTH_COMPARE_OP"))
	ECompareOp m_DepthCompareOp = { ECompareOp::Less };

	PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADER"), DISPLAY_NAME("PROP_VERTEX_SHADER_HANDLE"))
	ResourceHandle<Shader> m_VertexShaderHandle;

	PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADER"), DISPLAY_NAME("PROP_PIXEL_SHADER_HANDLE"))
	ResourceHandle<Shader> m_PixelShaderHandle;

	PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARAMETER"), DISPLAY_NAME("PROP_PARAMETERS"))
	unordered_map<string, MaterialParameter> m_Parameters = {};

	PROPERTY(EDITABLE, CATEGORY("CATEGORY_TEXTURE"), DISPLAY_NAME("PROP_TEXTURE_BINDINGS"))
	vector<MaterialTextureBinding> m_TextureBindings = {};

	PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADER"), DISPLAY_NAME("PROP_PASS_SHADERS"))
	vector<MaterialPassShader> m_PassShaders = {};

private:
	unordered_map<string, uint32> m_TextureNameToIndex = {};
	unordered_map<uint32, uint32> m_TextureSlotToIndex = {};
#pragma endregion
};
END