#pragma once
#include "MaterialInterface.h"
#include "SamplerManager.h"
#include "ResourceManager.h"
#include "Renderer.h"

#pragma region Cosntructor&Destructor
void MaterialInterface::Free()
{
	m_TextureBindings.clear();
	m_TextureNameToIndex.clear();
	m_TextureSlotToIndex.clear();
	m_Parameters.clear();
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
}

void MaterialInterface::AddFloat2(const string& name, const vec2& defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec2), 8);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float2, &defaultValue, sizeof(vec2), offset, min, max, localKey, false);
}

void MaterialInterface::AddFloat3(const string& name, const vec3& defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec3), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float3, &defaultValue, sizeof(vec3), offset, min, max, localKey, false);
}

void MaterialInterface::AddColor3(const string& name, const vec3& defaultColor, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec3), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float3, &defaultColor, sizeof(vec3), offset, 0.0f, 1.0f, localKey, true);
}

void MaterialInterface::AddFloat4(const string& name, const vec4& defaultValue, float min, float max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec4), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &defaultValue, sizeof(vec4), offset, min, max, localKey, false);
}

void MaterialInterface::AddColor4(const string& name, const vec4& defaultColor, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(vec4), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &defaultColor, sizeof(vec4), offset, 0.0f, 1.0f, localKey, true);
}

void MaterialInterface::AddInt(const string& name, int32 defaultValue, int32 min, int32 max, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(int32), 4);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Int, &defaultValue, sizeof(int32), offset, static_cast<float>(min), static_cast<float>(max), localKey, false);
}

void MaterialInterface::AddBool(const string& name, bool defaultValue, const string& localKey)
{
    uint32 offset = CalculateNextOffset(sizeof(bool), 4);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Bool, &defaultValue, sizeof(bool), offset, 0.0f, 1.0f, localKey, false);
}

void MaterialInterface::SetFloat(const string& name, f32 value)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end())
    {
        if (it->second.data.size() >= sizeof(value))
            memcpy(it->second.data.data(), &value, sizeof(value));
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
        return;
    }
    uint32 offset = CalculateNextOffset(sizeof(mat4), 16);
    m_Parameters[name] = MaterialParameter(EMaterialParameterType::Matrix, &value, sizeof(mat4), offset);
}

void MaterialInterface::ResetParameter(const string& name)
{
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end() && !it->second.defaultValue.empty())
        it->second.data = it->second.defaultValue;
}

void MaterialInterface::ResetAllParameters()
{
    for (auto& [name, param] : m_Parameters)
    {
        if (!param.defaultValue.empty())
            param.data = param.defaultValue;
    }
}

void MaterialInterface::ApplyParameterConstants(MaterialConstants& outConstants) const
{
    const auto findParameter = [this](const string& name) -> const MaterialParameter*
        {
            const auto it = m_Parameters.find(name);
            return it != m_Parameters.end() ? &it->second : nullptr;
        };

    if (const auto* baseColor = findParameter("BaseColor"); baseColor && baseColor->data.size() >= sizeof(vec4))
        outConstants.baseColor = *reinterpret_cast<const vec4*>(baseColor->data.data());
    else
        outConstants.baseColor = GetParameter<vec4>("Color", outConstants.baseColor);

    outConstants.roughness = GetParameter<float>("Roughness", outConstants.roughness);
    outConstants.metallic = GetParameter<float>("Metallic", outConstants.metallic);
    outConstants.cavity = GetParameter<float>("Cavity", outConstants.cavity);
    outConstants.ao = GetParameter<float>("AO", outConstants.ao);

    const auto* emissive = findParameter("EmissiveColor");
    if (!emissive) emissive = findParameter("Emission");
    if (!emissive) return;

    if (emissive->data.size() >= sizeof(vec4))
        outConstants.emissiveColor = vec3(*reinterpret_cast<const vec4*>(emissive->data.data()));
    else if (emissive->data.size() >= sizeof(vec3))
        outConstants.emissiveColor = *reinterpret_cast<const vec3*>(emissive->data.data());
}

#pragma endregion

#pragma region Texture Binding Interface
void MaterialInterface::SetTexture(const string& name, const ResourceHandle<Texture>& texture)
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index >= 0)
    {
        m_TextureBindings[index].texture = texture;
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
}
#pragma endregion

#pragma region Shader
// MaterialInterface.cpp 수정 제안
Shader* MaterialInterface::GetVertexShader(ERenderPassType passType) const
{
    for (const auto& binding : m_PassShaders)
    {
        if (binding.passType == passType && binding.vertexShader.IsValid() && binding.pixelShader.IsValid())
            return binding.vertexShader.Get();
    }
    return m_VertexShaderHandle.Get();
}

Shader* MaterialInterface::GetPixelShader(ERenderPassType passType) const
{
    for (const auto& binding : m_PassShaders)
    {
        if (binding.passType == passType && binding.vertexShader.IsValid() && binding.pixelShader.IsValid())
            return binding.pixelShader.Get();
    }
    return m_PixelShaderHandle.Get();
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
    return BlendState(m_BlendMode);
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
}

EResult MaterialInterface::Bind(uint32 slot)
{
    RHI* rhi = Renderer::Get().GetRHI();
    if (!rhi) return EResult::Fail;

    auto* defaultSampler = SamplerManager::Get().GetDefaultSampler();
    bool slotBound[4] = { false, false, false, false };

    // 1. 등록된 텍스처 바인딩
    for (auto& binding : m_TextureBindings)
    {
        Texture* texture = binding.texture.Get();
        if (!texture) continue;

        RHISampler* sampler = binding.hasCustomSampler
            ? SamplerManager::Get().GetOrCreateSampler(binding.samplerDesc)
            : defaultSampler;
        if (!sampler) return EResult::Fail;

        RHITexture* rhiTexture = texture->GetRHITexture();
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
            rhi->BindTexture(nullptr, s);
    }

    // 3. 머티리얼 상수 버퍼(MaterialConstants) 바인딩 (slot = 2 -> b0, space3)
    MaterialConstants constants;
    BuildMaterialConstants(constants);
    return rhi->BindConstantBuffer(&constants, sizeof(MaterialConstants), slot);
}
#pragma endregion