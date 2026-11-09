#include "ParticleEffect.h"
#include <archive/JsonArchive.h>
#include <algorithm>

namespace Engine
{
	EResult ParticleEffect::Initialize(void* arg)
	{
		if (arg)
		{
			auto* desc = static_cast<DESC*>(arg);
			m_Emitters = desc->emitters;
		}
		return EResult::Success;
	}

	ParticleEffect* ParticleEffect::Create(void* arg)
	{
		auto* instance = new ParticleEffect();
		if (instance->Initialize(arg) != EResult::Success)
		{
			delete instance;
			return nullptr;
		}
		return instance;
	}

	void ParticleEffect::Free()
	{
		m_Emitters.clear();
	}

	const ParticleEmitterData* ParticleEffect::GetEmitter(int32 index) const
	{
		if (index < 0 || index >= static_cast<int32>(m_Emitters.size())) return nullptr;
		return &m_Emitters[index];
	}

	ParticleEmitterData* ParticleEffect::GetEmitter(int32 index)
	{
		if (index < 0 || index >= static_cast<int32>(m_Emitters.size())) return nullptr;
		return &m_Emitters[index];
	}

	void ParticleEffect::AddEmitter(const ParticleEmitterData& emitterData)
	{
		m_Emitters.push_back(emitterData);
	}

	void ParticleEffect::InsertEmitter(int32 index, const ParticleEmitterData& emitterData)
	{
		const int32 clampedIndex = std::clamp(index, 0, static_cast<int32>(m_Emitters.size()));
		m_Emitters.insert(m_Emitters.begin() + clampedIndex, emitterData);
	}

	void ParticleEffect::RemoveEmitter(int32 index)
	{
		if (index >= 0 && index < static_cast<int32>(m_Emitters.size()))
			m_Emitters.erase(m_Emitters.begin() + index);
	}

	void ParticleEffect::ClearEmitters()
	{
		m_Emitters.clear();
	}

	bool ParticleEffect::SaveToFile(const std::wstring& filePath) const
	{
		archive::JsonArchive ar;
		if (!WriteArchive(ar)) return false;
		return static_cast<bool>(ar.SaveToFile(filePath));
	}

	bool ParticleEffect::LoadFromFile(const std::wstring& filePath)
	{
		archive::JsonArchive ar;
		if (!ar.LoadFromFile(filePath)) return false;
		return static_cast<bool>(ReadArchive(ar));
	}
}
