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
    std::filesystem::path targetPath = filePath;
    if (targetPath.empty())
        targetPath = m_Path.empty() ? std::filesystem::path(m_Key) : std::filesystem::path(m_Path);

    std::error_code ec;
    if (!std::filesystem::exists(targetPath, ec))
    {
        if (std::filesystem::exists(targetPath.string() + ".json", ec))
            targetPath = targetPath.string() + ".json";
        else
            return EResult::FileNotFound;
    }

    archive::ArchiveResult result;
    std::string ext = targetPath.extension().string();
    for (char& c : ext) c = static_cast<char>(tolower(c));

    if (ext == ".json")
    {
        archive::JsonArchive ar;
        result = ar.LoadFromFile(targetPath);
        if (!result)
        {
            ENGINE_LOG_ERROR("[Material Hot Reload] Failed to load JSON archive: {}", targetPath.string());
            return EResult::Fail;
        }
        result = ReadArchive(ar);
    }
    else
    {
        // .beve, .bammat, .bammatinst 등 BEVE 바이너리 아카이브 포맷
        archive::BeveArchive ar;
        result = ar.LoadFromFile(targetPath);
        if (!result)
        {
            ENGINE_LOG_ERROR("[Material Hot Reload] Failed to load binary archive: {}", targetPath.string());
            return EResult::Fail;
        }
        result = ReadArchive(ar);
    }

    if (!result)
    {
        ENGINE_LOG_ERROR("[Material Hot Reload] Failed to read archive data: {}", targetPath.string());
        return EResult::Fail;
    }

    OnPostLoad();
    IncreaseVersion();
    ENGINE_LOG_INFO("[Material Hot Reload] Successfully reloaded: {}", targetPath.string());
    return EResult::Success;
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
EResult MaterialInterface::Bind(uint32 slot)
{
    RHI* rhi = Renderer::Get().GetRHI();
    bool bBoundAny = false;
    for (auto& binding : m_TextureBindings)
    {
        Texture* texture = binding.texture.Get();
        if (!texture)
        {
            auto fallbackHandle = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png");
            texture = fallbackHandle.Get();
        }

        RHISampler* sampler = binding.hasCustomSampler
            ? SamplerManager::Get().GetOrCreateSampler(binding.samplerDesc)
            : SamplerManager::Get().GetDefaultSampler();

        if (!texture || !sampler) return EResult::Fail;

        RHITexture* rhiTexture = texture->GetRHITexture();
        if (!rhiTexture) continue;  // GPU 업로드 안 된 텍스처는 skip

        if (IsFailure(rhi->BindTextureSampler(rhiTexture, sampler, binding.slot)))
            return EResult::Fail;

        bBoundAny = true;
    }
    if (!bBoundAny)
    {
        auto fallbackHandle = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png");
        Texture* texture = fallbackHandle.Get();
        RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();
        if (!texture || !sampler) return EResult::Fail;

        RHITexture* rhiTexture = texture->GetRHITexture();
        if (!rhiTexture) return EResult::Fail;  // fallback도 GPU 미업로드 시 실패 처리

        if (IsFailure(rhi->BindTextureSampler(rhiTexture, sampler, 0)))
            return EResult::Fail;
    }
    return EResult::Success;
}
#pragma endregion