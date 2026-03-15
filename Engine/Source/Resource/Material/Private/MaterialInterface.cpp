#pragma once
#include "MaterialInterface.h"
#include "SamplerManager.h"
#include "ResourceManager.h"
#include "Renderer.h"

#pragma region Cosntructor&Destructor
void MaterialInterface::Free()
{
	for (auto& [name, textureSlot] : m_TextureSlots)
	{
		Safe_Release(textureSlot.texture);
		Safe_Release(textureSlot.sampler);
	}
	m_TextureSlots.clear();
	m_Parameters.clear();
}
#pragma endregion

void MaterialInterface::SetTexture(const string& name, RHITexture* texture)
{
	auto it = m_TextureSlots.find(name);
	if (it != m_TextureSlots.end())
	{
		Safe_Release(it->second.texture);
		it->second.texture = texture;
	}
	else
	{
		TextureSlot ts;
		ts.slot = static_cast<uint32>(m_TextureSlots.size());
		ts.texture = texture;
		m_TextureSlots[name] = ts;
	}
	Safe_AddRef(texture);
}

void MaterialInterface::SetTextureBySlot(uint32 slot, RHITexture* texture)
{
	string slotName = std::to_string(slot);
	auto it = m_TextureSlots.find(slotName);
	if (it != m_TextureSlots.end())
	{
		Safe_Release(it->second.texture);
		it->second.texture = texture;
	}
	else
	{
		TextureSlot ts;
		ts.slot = slot;
		ts.texture = texture;
		m_TextureSlots[slotName] = ts;
	}
	Safe_AddRef(texture);
}

void MaterialInterface::SetSampler(const string& name, RHISampler* sampler)
{
	auto it = m_TextureSlots.find(name);
	if (it == m_TextureSlots.end()) return;
	it->second.sampler = sampler;
	Safe_AddRef(sampler);
}

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

RHITexture* MaterialInterface::GetTexture(const string& name) const
{
	auto it = m_TextureSlots.find(name);
	if (it != m_TextureSlots.end())
		return it->second.texture;
	return nullptr;
}

RHITexture* MaterialInterface::GetTextureBySlot(uint32 slot) const
{
	for (auto& [name, textureSlot] : m_TextureSlots)
	{
		if (textureSlot.slot == slot)
			return textureSlot.texture;
	}
	return nullptr;
}

RHISampler* MaterialInterface::GetSampler(const string& name) const
{
	auto it = m_TextureSlots.find(name);
	if (it != m_TextureSlots.end())
		return it->second.sampler;
	return nullptr;
}
#pragma endregion


#pragma region Bind
EResult MaterialInterface::Bind(uint32 slot)
{
	RHI* rhi = Renderer::Get().GetRHI();
	for (auto& [name, textureSlot] : m_TextureSlots)
	{
		RHITexture* texture = textureSlot.texture;
		if (!texture)
			texture = ResourceManager::Get().GetTexture(L"DefaultTexture")->GetRHITexture();
		RHISampler* sampler = textureSlot.sampler;
		if (!sampler)
			sampler = SamplerManager::Get().GetDefaultSampler();
		if (IsFailure(rhi->BindTextureSampler(texture, sampler, textureSlot.slot)))
			return EResult::Fail;
	}
	return EResult::Success;
}
#pragma endregion
