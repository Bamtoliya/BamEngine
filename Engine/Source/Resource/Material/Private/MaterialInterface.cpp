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
#pragma endregion

#pragma region Parameter Interface
template<typename T>
T MaterialInterface::GetParameter(const string& name) const
{
	auto it = m_Parameters.find(name);
	if (it != m_Parameters.end() && it->second.data.size() >= sizeof(T))
	{
		T value;
		memcpy(&value, it->second.data.data(), sizeof(T));
		return value;
	}
	return T{};
}

// 명시적 인스턴스화
template f32 MaterialInterface::GetParameter<f32>(const string&) const;
template vec2 MaterialInterface::GetParameter<vec2>(const string&) const;
template vec3 MaterialInterface::GetParameter<vec3>(const string&) const;
template vec4 MaterialInterface::GetParameter<vec4>(const string&) const;
template int32 MaterialInterface::GetParameter<int32>(const string&) const;
template bool MaterialInterface::GetParameter<bool>(const string&) const;
template mat4 MaterialInterface::GetParameter<mat4>(const string&) const;
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

void MaterialInterface::SetSamplerDesc(const string& name, const tagSamplerDesc& desc)
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

void MaterialInterface::SetSamplerDescBySlot(uint32 slot, const tagSamplerDesc& desc)
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

const Engine::tagSamplerDesc* MaterialInterface::GetSamplerDesc(const string& name) const
{
    int32 index = FindTextureBindingIndexByName(name);
    if (index >= 0 && m_TextureBindings[index].hasCustomSampler)
    {
        return &m_TextureBindings[index].samplerDesc;
    }
    return nullptr;
}

const Engine::tagSamplerDesc* MaterialInterface::GetSamplerDescBySlot(uint32 slot) const
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

#pragma region Getter
tagBlendState MaterialInterface::GetBlendState()
{
    return tagBlendState(m_BlendMode);
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
            texture = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png").Get();

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
        Texture* texture = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png").Get();
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

#pragma region Save&Load
void MaterialInterface::Deserialize(Archive& ar) 
{
	Resource::Deserialize(ar);

	for (auto& binding : m_TextureBindings)
	{
		if (binding.name.empty())
		{
			binding.name = std::to_string(binding.slot);
		}
	}

	RebuildTextureBindingCache();
}
#pragma endregion
