#include "ActionTimelineAsset.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

BEGIN(Engine)

EResult ActionTimelineAsset::Initialize(void* arg)
{
	if (IsFailure(Resource::Initialize(arg))) return EResult::Fail;
	if (arg)
	{
		CAST_DESC;
		m_ModelPath = desc->ModelPath;
		m_ClipName = desc->ClipName;
		m_Duration = desc->Duration;
		m_NotifyTracks = desc->NotifyTracks;
		m_CameraTracks = desc->CameraTracks;
	}
	return EResult::Success;
}

void ActionTimelineAsset::Free()
{
	m_NotifyTracks.clear();
	m_CameraTracks.clear();
}

ActionTimelineAsset* ActionTimelineAsset::Create(void* arg)
{
	auto* asset = new ActionTimelineAsset();
	if (IsFailure(asset->Initialize(arg)))
	{
		delete asset;
		return nullptr;
	}
	return asset;
}

bool ActionTimelineAsset::SaveToFile(const std::string& filePath) const
{
	const std::filesystem::path fullPath = PlatformPath::ResolveProjectRelative(filePath);
	if (fullPath.has_parent_path())
	{
		std::filesystem::create_directories(fullPath.parent_path());
	}

	archive::JsonArchive ar;
	// Resource 베이스 클래스의 리플렉션 직렬화 자동 수행
	const auto writeRes = WriteArchive(ar);
	if (!writeRes) return false;

	const auto saveRes = ar.SaveToFile(fullPath);
	return static_cast<bool>(saveRes);
}

bool ActionTimelineAsset::LoadFromFile(const std::string& filePath)
{
	const std::filesystem::path fullPath = PlatformPath::ResolveProjectRelative(filePath);
	archive::JsonArchive ar;
	const auto loadRes = ar.LoadFromFile(fullPath);
	if (!loadRes) return false;

	// Resource 베이스 클래스의 리플렉션 역직렬화 자동 수행 (this 객체에 직접 복원)
	const auto readRes = ReadArchive(ar);
	return static_cast<bool>(readRes);
}

END