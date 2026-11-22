#pragma once
#include "MaterialInterface.h"
#include "SamplerManager.h"
#include "ResourceManager.h"
#include "ResourceStreamer.h"
#include "Renderer.h"
#include <string_view>

namespace
{
	bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b)
	{
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i)
		{
			unsigned char ca = static_cast<unsigned char>(a[i]);
			unsigned char cb = static_cast<unsigned char>(b[i]);
			if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + ('a' - 'A'));
			if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + ('a' - 'A'));
			if (ca != cb) return false;
		}
		return true;
	}

	uint32 FeatureFlagForTextureSlot(uint32 slot)
	{
		switch (slot)
		{
		case 0: return static_cast<uint32>(EMaterialFeatureFlags::HasDiffuse);
		case 1: return static_cast<uint32>(EMaterialFeatureFlags::HasNormal);
		case 2: return static_cast<uint32>(EMaterialFeatureFlags::HasPBR);
		case 3: return static_cast<uint32>(EMaterialFeatureFlags::HasEmissive);
		default: return 0u;
		}
	}
}

const MaterialParameterEntry* MaterialBufferLayout::Find(const string& name) const
{
	for (const auto& entry : Entries)
	{
		if (EqualsIgnoreCaseAscii(entry.Name, name))
			return &entry;
	}
	return nullptr;
}

#pragma region Cosntructor&Destructor
void MaterialInterface::Free()
{
	m_TextureBindings.clear();
	m_TextureNameToIndex.clear();
	m_TextureSlotToIndex.clear();
	m_Parameters.clear();
	m_ShaderGraphPath.clear();
	m_BufferLayout.Clear();
	m_TextureDescriptors.clear();
	m_CachedConstantBlob.clear();
	m_CachedConstantVersion = 0;
	m_CachedTextureFlagMask = UINT32_MAX;
	MarkConstantBlobDirty();
}

EResult MaterialInterface::Reload(const std::filesystem::path& filePath)
{
    const auto result = Resource::Reload(filePath);
    if (result != EResult::FileNotFound)
        return result;

    const auto& storedPath = m_Path.empty() ? m_Key : m_Path;
    auto fallbackPath = filePath.empty() ? std::filesystem::path(storedPath) : filePath;
    fallbackPath += L".json";
    return Resource::Reload(fallbackPath);
}
#pragma endregion

#pragma region Internal Helpers
void MaterialInterface::RebuildTextureBindingCache()
{
    m_TextureNameToIndex.clear();
    m_TextureSlotToIndex.clear();

    for (uint32 i = 0; i < m_TextureBindings.size(); ++i)
    {
        auto& binding = m_TextureBindings[i];

        if (binding.name.empty())
        {
            binding.name = std::to_string(binding.slot);
        }

        m_TextureNameToIndex[binding.name] = i;
        m_TextureSlotToIndex[binding.slot] = i;
    }
    MarkConstantBlobDirty();
}

int32 MaterialInterface::FindTextureBindingIndexByName(const string& name) const
{
    auto it = m_TextureNameToIndex.find(name);
    if (it != m_TextureNameToIndex.end())
    {
        return static_cast<int32>(it->second);
    }
    return -1;
}

int32 MaterialInterface::FindTextureBindingIndexBySlot(uint32 slot) const
{
    auto it = m_TextureSlotToIndex.find(slot);
    if (it != m_TextureSlotToIndex.end())
    {
        return static_cast<int32>(it->second);
    }
    return -1;
}

EResult MaterialInterface::OnPostLoad()
{
    RebuildTextureBindingCache();

    // 레거시 에셋(.bammat.json) 하위 호환: m_PrimaryPassType 필드가 없어 기본값(Geometry)으로 로드된
    // 특수 패스 머티리얼(UI, PostProcess, 반투명 ForwardTransparent)의 주 패스를 안전하게 보정한다.
    if (m_PrimaryPassType == ERenderPassType::Geometry)
    {
        auto shaderKeyLower = [](const ResourceHandle<Shader>& handle) -> string
        {
            if (auto* s = handle.Get())
                return Engine::ToLowerStr(Engine::WStrToStr(s->GetKey()));
            return {};
        };

        const string psKey = shaderKeyLower(m_PixelShaderHandle);
        const string vsKey = shaderKeyLower(m_VertexShaderHandle);

        if (psKey.find("uips") != string::npos || vsKey.find("uivs") != string::npos)
        {
            m_PrimaryPassType = ERenderPassType::UI;
        }
        else if (psKey.find("postprocess") != string::npos || vsKey.find("fullscreenquad") != string::npos)
        {
            m_PrimaryPassType = ERenderPassType::PostProcess;
        }
        else if (m_BlendMode == EBlendMode::Forward || psKey.find("skyps") != string::npos || vsKey.find("skyvs") != string::npos)
        {
            m_PrimaryPassType = ERenderPassType::Forward;
        }
        else if (m_BlendMode == EBlendMode::AlphaBlend || m_BlendMode == EBlendMode::Additive ||
                 m_BlendMode == EBlendMode::NonPremultiplied || m_BlendMode == EBlendMode::Decal)
        {
            const bool isGBufferPs = (psKey.find("gbuffer") != string::npos);
            if (!isGBufferPs)
                m_PrimaryPassType = ERenderPassType::ForwardTransparent;
        }
    }

    return EResult::Success;
}
#pragma endregion

#pragma region Parameter Interface

uint32 MaterialInterface::CalculateNextOffset(size_t typeSize, size_t alignment) const
{
    size_t currentEnd = 0;
    for (const auto& [name, param] : m_Parameters)
    {
        const size_t end = param.offset + param.data.size();
        if (end > currentEnd) currentEnd = end;
    }

    // 1. 기본 정렬 올림 (4B, 8B, 16B)
    size_t alignedOffset = (currentEnd + (alignment - 1)) & ~(alignment - 1);

    // 2. HLSL 16바이트 경계 넘침(Crossing 16-byte boundary) 방지 검사
    if ((alignedOffset / 16) != ((alignedOffset + typeSize - 1) / 16))
    {
        alignedOffset = (alignedOffset + 15) & ~15; // 다음 16바이트 경계로 점프
    }

    return static_cast<uint32>(alignedOffset);
}

void MaterialInterface::AddFloat(const string& name, f32 defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(f32), 4);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &defaultValue, sizeof(f32), offset, min, max, localKey, false);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddFloat2(const string& name, const vec2& defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec2), 8);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float2, &defaultValue, sizeof(vec2), offset, min, max, localKey, false);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddFloat3(const string& name, const vec3& defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec3), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float3, &defaultValue, sizeof(vec3), offset, min, max, localKey, false);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddColor3(const string& name, const vec3& defaultColor, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec3), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float3, &defaultColor, sizeof(vec3), offset, 0.0f, 1.0f, localKey, true);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddFloat4(const string& name, const vec4& defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec4), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &defaultValue, sizeof(vec4), offset, min, max, localKey, false);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddColor4(const string& name, const vec4& defaultColor, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec4), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &defaultColor, sizeof(vec4), offset, 0.0f, 1.0f, localKey, true);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddInt(const string& name, int32 defaultValue, int32 min, int32 max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(int32), 4);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Int, &defaultValue, sizeof(int32), offset, static_cast<float>(min), static_cast<float>(max), localKey, false);
    MarkConstantBlobDirty();
}

void MaterialInterface::AddBool(const string& name, bool defaultValue, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(bool), 4);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Bool, &defaultValue, sizeof(bool), offset, 0.0f, 1.0f, localKey, false);
    MarkConstantBlobDirty();
}

void MaterialInterface::SetFloat(const string& name, f32 value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    AddFloat(name, value);
}

void MaterialInterface::SetFloat2(const string& name, const vec2& value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    AddFloat2(name, value);
}

void MaterialInterface::SetFloat3(const string& name, const vec3& value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    AddFloat3(name, value);
}

void MaterialInterface::SetFloat4(const string& name, const vec4& value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    AddFloat4(name, value);
}

void MaterialInterface::SetInt(const string& name, int32 value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    AddInt(name, value);
}

void MaterialInterface::SetBool(const string& name, bool value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    AddBool(name, value);
}

void MaterialInterface::SetMatrix(const string& name, const mat4& value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
        MarkConstantBlobDirty();
        return;
    }
    uint32 offset = CalculateNextOffset(sizeof(mat4), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Matrix, &value, sizeof(mat4), offset);
    MarkConstantBlobDirty();
}

void MaterialInterface::ResetParameter(const string& name)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end() && !it->second.defaultValue.empty())
    {
        it->second.data = it->second.defaultValue;
        MarkConstantBlobDirty();
    }
}

void MaterialInterface::ResetAllParameters()
{
    for (auto& [name, param] : m_Parameters)
    {
        if (!param.defaultValue.empty())
            param.data = param.defaultValue;
    }
    MarkConstantBlobDirty();
}

std::optional<f32> MaterialInterface::FindAlphaCutoffParameter() const
{
    for (const auto& [key, param] : m_Parameters)
    {
        if (param.type == EMaterialParameterType::Float && param.data.size() >= sizeof(f32) && EqualsIgnoreCaseAscii(key, "AlphaCutoff"))
            return *reinterpret_cast<const f32*>(param.data.data());
    }
    return std::nullopt;
}

f32 MaterialInterface::GetAlphaCutoff() const
{
    return FindAlphaCutoffParameter().value_or(DEFAULT_ALPHA_CUTOFF);
}

void MaterialInterface::SetAlphaCutoff(f32 cutoff)
{
    for (const auto& [key, param] : m_Parameters)
    {
        if (param.type == EMaterialParameterType::Float && EqualsIgnoreCaseAscii(key, "AlphaCutoff"))
        {
            const string existing = key;
            SetFloat(existing, cutoff);
            return;
        }
    }
    AddFloat("AlphaCutoff", DEFAULT_ALPHA_CUTOFF);
    SetFloat("AlphaCutoff", cutoff);
}

void MaterialInterface::ApplyParameterConstants(MaterialConstants& outConstants) const
{
    if (m_Parameters.empty()) return;

    const MaterialParameter* baseColorParam = nullptr;
    const MaterialParameter* colorParam = nullptr;
    const MaterialParameter* emissiveColorParam = nullptr;
    const MaterialParameter* emissionParam = nullptr;
    const MaterialParameter* emissiveParam = nullptr;

    // 단일 순회 + 무할당 대소문자 무시 비교로 드로우콜당 문자열 할당 제거
    for (const auto& [key, param] : m_Parameters)
    {
        if (EqualsIgnoreCaseAscii(key, "BaseColor"))
            baseColorParam = &param;
        else if (EqualsIgnoreCaseAscii(key, "Color"))
            colorParam = &param;
        else if (EqualsIgnoreCaseAscii(key, "Roughness") && param.data.size() >= sizeof(float))
            outConstants.roughness = *reinterpret_cast<const float*>(param.data.data());
        else if (EqualsIgnoreCaseAscii(key, "Metallic") && param.data.size() >= sizeof(float))
            outConstants.metallic = *reinterpret_cast<const float*>(param.data.data());
        else if (EqualsIgnoreCaseAscii(key, "Cavity") && param.data.size() >= sizeof(float))
            outConstants.cavity = *reinterpret_cast<const float*>(param.data.data());
        else if (EqualsIgnoreCaseAscii(key, "AO") && param.data.size() >= sizeof(float))
            outConstants.ao = *reinterpret_cast<const float*>(param.data.data());
        else if (EqualsIgnoreCaseAscii(key, "EmissiveColor"))
            emissiveColorParam = &param;
        else if (EqualsIgnoreCaseAscii(key, "Emission"))
            emissionParam = &param;
        else if (EqualsIgnoreCaseAscii(key, "Emissive"))
            emissiveParam = &param;
    }

    if (baseColorParam && baseColorParam->data.size() >= sizeof(vec4))
        outConstants.baseColor = *reinterpret_cast<const vec4*>(baseColorParam->data.data());
    else if (colorParam && colorParam->data.size() >= sizeof(vec4))
        outConstants.baseColor = *reinterpret_cast<const vec4*>(colorParam->data.data());

    const MaterialParameter* emissive = emissiveColorParam ? emissiveColorParam : (emissionParam ? emissionParam : emissiveParam);
    if (emissive)
    {
        if (emissive->data.size() >= sizeof(vec4))
            outConstants.emissiveColor = vec3(*reinterpret_cast<const vec4*>(emissive->data.data()));
        else if (emissive->data.size() >= sizeof(vec3))
            outConstants.emissiveColor = *reinterpret_cast<const vec3*>(emissive->data.data());
    }
}

void MaterialInterface::ApplyCustomParametersToBlob(vector<uint8>& outBlob, const MaterialBufferLayout& layout) const
{
    if (!layout.HasCustomParameters() || m_Parameters.empty()) return;

    const auto findParamCaseInsensitive = [this](std::string_view name) -> const MaterialParameter*
    {
        for (const auto& [key, param] : m_Parameters)
        {
            if (EqualsIgnoreCaseAscii(key, name))
                return &param;
        }
        return nullptr;
    };

    for (const auto& entry : layout.Entries)
    {
        if (entry.Offset < sizeof(MaterialConstants) || entry.Offset > outBlob.size() ||
            entry.Size > outBlob.size() - entry.Offset) continue;

        const auto* param = findParamCaseInsensitive(entry.Name);
        if (!param || param->type != entry.Type || param->data.empty()) continue;

        if (entry.Type == EMaterialParameterType::Bool && entry.Size == sizeof(uint32))
        {
            const uint32 boolVal = (param->data[0] != 0) ? 1u : 0u;
            memcpy(outBlob.data() + entry.Offset, &boolVal, sizeof(uint32));
        }
        else
        {
            const size_t copySize = (std::min)(static_cast<size_t>(entry.Size), param->data.size());
            memcpy(outBlob.data() + entry.Offset, param->data.data(), copySize);
        }
    }
}

void MaterialInterface::BuildConstantBufferBlob(vector<uint8>& outBlob) const
{
    const size_t totalSize = std::clamp<size_t>(m_BufferLayout.TotalBufferSize,
        sizeof(MaterialConstants), MaterialBufferLayout::MaxBufferSize);
    const size_t alignedSize = (totalSize + 15u) & ~static_cast<size_t>(15u);
    outBlob.assign(alignedSize, 0u);

    MaterialConstants baseConstants;
    BuildMaterialConstants(baseConstants);
    memcpy(outBlob.data(), &baseConstants, sizeof(MaterialConstants));

    ApplyCustomParametersToBlob(outBlob, m_BufferLayout);
}

const vector<uint8>& MaterialInterface::GetOrBuildConstantBufferBlob(uint32 liveTextureFlagMask) const
{
    if (m_CachedConstantBlob.empty() || m_CachedConstantVersion != m_ConstantVersion ||
        m_CachedTextureFlagMask != liveTextureFlagMask)
    {
        BuildConstantBufferBlob(m_CachedConstantBlob);
        m_CachedConstantVersion = m_ConstantVersion;
        m_CachedTextureFlagMask = liveTextureFlagMask;
    }
    return m_CachedConstantBlob;
}

#pragma endregion

#pragma region Texture Binding Interface
void MaterialInterface::SetTexture(const string& name, const ResourceHandle<Texture>& texture)
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index >= 0)
    {
        m_TextureBindings[index].texture = texture;
        MarkConstantBlobDirty();
        return;
    }

    MaterialTextureBinding binding;
    binding.name = name;
    binding.slot = static_cast<uint32>(m_TextureBindings.size());
    binding.texture = texture;

    m_TextureBindings.push_back(binding);
    RebuildTextureBindingCache();
}

void MaterialInterface::SetTextureBySlot(uint32 slot, const ResourceHandle<Texture>& texture)
{
    int32 index = FindTextureBindingIndexBySlot(slot);
    if (index >= 0)
    {
        m_TextureBindings[index].texture = texture;
        MarkConstantBlobDirty();
        return;
    }

    MaterialTextureBinding binding;
    binding.slot = slot;
    binding.name = std::to_string(slot);
    binding.texture = texture;

    m_TextureBindings.push_back(binding);
    RebuildTextureBindingCache();
}

void MaterialInterface::SetTextureBinding(const string& name, uint32 slot, const ResourceHandle<Texture>& texture)
{
    int32 nameIndex = FindTextureBindingIndexByName(name);
    int32 slotIndex = FindTextureBindingIndexBySlot(slot);

    if (nameIndex >= 0 && slotIndex >= 0)
    {
        if (nameIndex == slotIndex)
        {
            m_TextureBindings[nameIndex].name = name;
            m_TextureBindings[nameIndex].slot = slot;
            m_TextureBindings[nameIndex].texture = texture;
            MarkConstantBlobDirty();
            return;
        }

        m_TextureBindings[nameIndex].name = name;
        m_TextureBindings[nameIndex].slot = slot;
        m_TextureBindings[nameIndex].texture = texture;

        m_TextureBindings.erase(m_TextureBindings.begin() + slotIndex);
        RebuildTextureBindingCache();
        return;
    }

    if (nameIndex >= 0)
    {
        m_TextureBindings[nameIndex].name = name;
        m_TextureBindings[nameIndex].slot = slot;
        m_TextureBindings[nameIndex].texture = texture;
        RebuildTextureBindingCache();
        return;
    }

    if (slotIndex >= 0)
    {
        m_TextureBindings[slotIndex].name = name;
        m_TextureBindings[slotIndex].slot = slot;
        m_TextureBindings[slotIndex].texture = texture;
        RebuildTextureBindingCache();
        return;
    }

    MaterialTextureBinding binding;
    binding.name = name;
    binding.slot = slot;
    binding.texture = texture;

    m_TextureBindings.push_back(binding);
    RebuildTextureBindingCache();
}

void MaterialInterface::SetSamplerDesc(const string& name, const SamplerDesc& desc)
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index < 0)
    {
        MaterialTextureBinding binding;
        binding.name = name;
        binding.slot = static_cast<uint32>(m_TextureBindings.size());
        binding.hasCustomSampler = true;
        binding.samplerDesc = desc;
        m_TextureBindings.push_back(binding);
        RebuildTextureBindingCache();
        return;
    }

    m_TextureBindings[index].hasCustomSampler = true;
    m_TextureBindings[index].samplerDesc = desc;
}

void MaterialInterface::SetSamplerDescBySlot(uint32 slot, const SamplerDesc& desc)
{
    int32 index = FindTextureBindingIndexBySlot(slot);
    if (index < 0)
    {
        MaterialTextureBinding binding;
        binding.slot = slot;
        binding.name = std::to_string(slot);
        binding.hasCustomSampler = true;
        binding.samplerDesc = desc;
        m_TextureBindings.push_back(binding);
        RebuildTextureBindingCache();
        return;
    }

    m_TextureBindings[index].hasCustomSampler = true;
    m_TextureBindings[index].samplerDesc = desc;
}

Texture* MaterialInterface::GetTexture(const string& name) const
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index >= 0)
    {
        return m_TextureBindings[index].texture.Get();
    }
    return nullptr;
}

Texture* MaterialInterface::GetTextureBySlot(uint32 slot) const
{
    int32 index = FindTextureBindingIndexBySlot(slot);
    if (index >= 0)
    {
        return m_TextureBindings[index].texture.Get();
    }
    return nullptr;
}

ResourceHandle<Texture> MaterialInterface::GetTextureHandle(const string& name) const
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index >= 0)
    {
        return m_TextureBindings[index].texture;
    }
    return ResourceHandle<Texture>();
}

ResourceHandle<Texture> MaterialInterface::GetTextureHandleBySlot(uint32 slot) const
{
    int32 index = FindTextureBindingIndexBySlot(slot);
    if (index >= 0)
    {
        return m_TextureBindings[index].texture;
    }
    return ResourceHandle<Texture>();
}

const Engine::SamplerDesc* MaterialInterface::GetSamplerDesc(const string& name) const
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index >= 0 && m_TextureBindings[index].hasCustomSampler)
    {
        return &m_TextureBindings[index].samplerDesc;
    }
    return nullptr;
}

const Engine::SamplerDesc* MaterialInterface::GetSamplerDescBySlot(uint32 slot) const
{
    int32 index = FindTextureBindingIndexBySlot(slot);
    if (index >= 0 && m_TextureBindings[index].hasCustomSampler)
    {
        return &m_TextureBindings[index].samplerDesc;
    }
    return nullptr;
}

bool MaterialInterface::HasTextureBinding(const string& name) const
{
    return FindTextureBindingIndexByName(name) >= 0;
}

bool MaterialInterface::HasTextureBindingBySlot(uint32 slot) const
{
    return FindTextureBindingIndexBySlot(slot) >= 0;
}

void MaterialInterface::RemoveTextureBinding(const string& name)
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index < 0) return;

    m_TextureBindings.erase(m_TextureBindings.begin() + index);
    RebuildTextureBindingCache();
}

void MaterialInterface::RemoveTextureBindingBySlot(uint32 slot)
{
    int32 index = FindTextureBindingIndexBySlot(slot);
    if (index < 0) return;

    m_TextureBindings.erase(m_TextureBindings.begin() + index);
    RebuildTextureBindingCache();
}

void MaterialInterface::ClearTextureBindings()
{
    m_TextureBindings.clear();
    m_TextureNameToIndex.clear();
    m_TextureSlotToIndex.clear();
    MarkConstantBlobDirty();
}

void MaterialInterface::SyncTextureBindings(const vector<MaterialTextureDescriptor>& descriptors)
{
    m_TextureDescriptors = descriptors;
    for (const auto& desc : descriptors)
    {
        int32 existingIndex = FindTextureBindingIndexBySlot(desc.Slot);
        if (existingIndex < 0)
        {
            string bindingName = desc.Name.empty() ? ("TextureSlot" + std::to_string(desc.Slot)) : desc.Name;
            SetTextureBinding(bindingName, desc.Slot, {});
        }
        else
        {
            if (!desc.Name.empty() && (m_TextureBindings[existingIndex].name.empty() ||
                m_TextureBindings[existingIndex].name.rfind("TextureSlot", 0) == 0))
            {
                m_TextureBindings[existingIndex].name = desc.Name;
                RebuildTextureBindingCache();
            }
        }
    }
}
#pragma endregion

#pragma region Shader
Shader* MaterialInterface::GetVertexShader(ERenderPassType passType) const
{
    for (const auto& binding : m_PassShaders)
    {
        if (binding.passType == passType && binding.vertexShader.IsValid() && binding.pixelShader.IsValid())
            return binding.vertexShader.Get();
    }
    if (passType == m_PrimaryPassType || m_PrimaryPassType == ERenderPassType::Custom)
        return m_VertexShaderHandle.Get();
    return nullptr;
}

Shader* MaterialInterface::GetPixelShader(ERenderPassType passType) const
{
    for (const auto& binding : m_PassShaders)
    {
        if (binding.passType == passType && binding.vertexShader.IsValid() && binding.pixelShader.IsValid())
            return binding.pixelShader.Get();
    }
    if (passType == m_PrimaryPassType || m_PrimaryPassType == ERenderPassType::Custom)
        return m_PixelShaderHandle.Get();
    return nullptr;
}

void MaterialInterface::SetPassShader(ERenderPassType passType, const ResourceHandle<Shader>& vs, const ResourceHandle<Shader>& ps)
{
    for (auto& binding : m_PassShaders)
    {
        if (binding.passType == passType)
        {
            binding.vertexShader = vs;
            binding.pixelShader = ps;
            return;
        }
    }
    m_PassShaders.push_back({ passType, vs, ps });
}

bool MaterialInterface::HasPassShader(ERenderPassType passType) const
{
    for (const auto& binding : m_PassShaders)
    {
        if (binding.passType == passType)
            return true;
    }
    return false;
}

void MaterialInterface::RemovePassShader(ERenderPassType passType)
{
    std::erase_if(m_PassShaders, [passType](const MaterialPassShader& item) { return item.passType == passType; });
}
#pragma endregion

#pragma region Getter
BlendState MaterialInterface::GetBlendState()
{
    return BlendState(GetBlendMode());
}
#pragma endregion

#pragma region Bind
void MaterialInterface::BuildMaterialConstants(MaterialConstants& outConstants) const
{
    outConstants = MaterialConstants{};

    // 1. 등록된 유효 텍스처 슬롯에 따라 비트 플래그 설정
    for (const auto& binding : m_TextureBindings)
    {
        if (!binding.texture.Get()) continue;
        switch (binding.slot)
        {
        case 0: outConstants.materialFlags |= static_cast<uint32>(EMaterialFeatureFlags::HasDiffuse); break;
        case 1: outConstants.materialFlags |= static_cast<uint32>(EMaterialFeatureFlags::HasNormal); break;
        case 2: outConstants.materialFlags |= static_cast<uint32>(EMaterialFeatureFlags::HasPBR); break;
        case 3: outConstants.materialFlags |= static_cast<uint32>(EMaterialFeatureFlags::HasEmissive); break;
        default: break;
        }
    }

    ApplyParameterConstants(outConstants);
    outConstants.materialFlags = PackMaterialAlphaCutoff(outConstants.materialFlags,
        GetBlendMode() == EBlendMode::Masked, GetAlphaCutoff());
}

EResult MaterialInterface::Bind(uint32 slot)
{
    RHI* rhi = Renderer::Get().GetRHI();
    if (!rhi) return EResult::Fail;

    auto* defaultSampler = SamplerManager::Get().GetDefaultSampler();
    bool slotBound[4] = { false, false, false, false };
    uint32 liveTextureFlagMask = 0u;

    // 1. 등록된 텍스처 바인딩
    for (auto& binding : m_TextureBindings)
    {
        Texture* texture = binding.texture.Get();
        if (!texture) continue;

        liveTextureFlagMask |= FeatureFlagForTextureSlot(binding.slot);

        RHISampler* sampler = binding.hasCustomSampler
            ? SamplerManager::Get().GetOrCreateSampler(binding.samplerDesc)
            : defaultSampler;
        if (!sampler) return EResult::Fail;

        ResourceStreamer::NoteBound(*texture);
        RHITexture* rhiTexture = texture->GetAvailableRHITexture();
        if (!rhiTexture) continue;

        if (IsFailure(rhi->BindTextureSampler(rhiTexture, sampler, binding.slot)))
            return EResult::Fail;

        if (binding.slot < 4)
            slotBound[binding.slot] = true;
    }

    // 2. [상태 누수 방지]: 머티리얼에 지정되지 않은 빈 슬롯을 nullptr로 해제하여 이전 드로우콜 잔류 방지
    for (uint32 s = 0; s < 4; ++s)
    {
        if (!slotBound[s])
            (void)rhi->BindTexture(nullptr, s);
    }

    // 3. 캐시된 머티리얼 상수 버퍼 바인딩 (무할당 재사용, 변경 시에만 재구성)
    const vector<uint8>& constantBlob = GetOrBuildConstantBufferBlob(liveTextureFlagMask);
    return rhi->BindConstantBuffer(constantBlob.data(), static_cast<uint32>(constantBlob.size()), slot);
}
#pragma endregion
