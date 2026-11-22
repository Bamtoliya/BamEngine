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
		if (!desc->Path.empty())
		{
			if (!LoadFromFile(WStrToStr(desc->Path)))
			{
				return EResult::Fail;
			}
		}
		else
		{
			m_ModelPath = desc->ModelPath;
			m_ClipName = desc->ClipName;
			m_Duration = desc->Duration;
			m_NotifyTracks = desc->NotifyTracks;
			for (auto& track : m_NotifyTracks) track.Kind = ResolveTrackKind(track);
			m_CameraTracks = desc->CameraTracks;
			m_CameraCutTracks = desc->CameraCutTracks;
			m_CameraSplineTracks = desc->CameraSplineTracks;
		}
	}
	return EResult::Success;
}

void ActionTimelineAsset::Free()
{
	m_NotifyTracks.clear();
	m_CameraTracks.clear();
	m_CameraCutTracks.clear();
	m_CameraSplineTracks.clear();
}

void ActionTimelineAsset::PrepareNotifyPathRelocation(std::vector<TimelineNotifyTrack>& tracks,
	const vector<FilePathChange>& changes, vector<Utf8FilePathUpdate>& updates)
{
	for (auto& track : tracks)
		for (auto& notify : track.Notifies)
		{
			PlatformPath::PrepareUtf8FilePathUpdate(notify.SoundPath, changes, updates, true);
			PlatformPath::PrepareUtf8FilePathUpdate(notify.VFXPath, changes, updates, true);
		}
}

bool ActionTimelineAsset::PrepareFilePathRelocation(const vector<FilePathChange>& changes,
	vector<Utf8FilePathUpdate>& updates)
{
	const auto previousSize = updates.size();
	PlatformPath::PrepareUtf8FilePathUpdate(m_ModelPath, changes, updates, true);
	PrepareNotifyPathRelocation(m_NotifyTracks, changes, updates);
	return updates.size() == previousSize || GetPath().empty() || IsProjectJsonDocument(GetPath());
}

bool ActionTimelineAsset::IsProjectJsonDocument(const std::filesystem::path& pathOrUri)
{
	const auto path = PlatformPath::ResolveUri(pathOrUri);
	const auto extension = ToLowerStr(WStrToStr(path.extension().wstring()));
	return PlatformPath::IsProjectUri(PlatformPath::ToVirtualUri(path)) &&
		(extension == ".json" || extension == ".actiontimeline");
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

ETimelineNotifyTrackKind ActionTimelineAsset::ResolveTrackKind(const TimelineNotifyTrack& track)
{
	if (track.Kind != ETimelineNotifyTrackKind::Unspecified) return track.Kind;
	const auto& name = track.TrackName;
	if (name.find("Audio") != std::string::npos || name.find("Sound") != std::string::npos) return ETimelineNotifyTrackKind::Audio;
	if (name.find("Event") != std::string::npos) return ETimelineNotifyTrackKind::Event;
	return ETimelineNotifyTrackKind::Effect;
}

bool ActionTimelineAsset::SaveToFile(const std::string& filePath) const
{
	const auto path = Utf8ToPath(filePath);
	const auto fullPath = PlatformPath::IsVirtualUri(filePath)
		? PlatformPath::ResolveUri(path) : PlatformPath::ResolveProjectRelative(path);
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
	const auto path = Utf8ToPath(filePath);
	std::filesystem::path fullPath = PlatformPath::ResolveUri(path);
	if (!std::filesystem::exists(fullPath))
		fullPath = PlatformPath::ResolveProjectRelative(path);
	archive::JsonArchive ar;
	const auto loadRes = ar.LoadFromFile(fullPath);
	if (!loadRes) return false;

	// Resource 베이스 클래스의 리플렉션 역직렬화 자동 수행 (this 객체에 직접 복원). 트랙 종류 판정은 OnPostLoad에서 수행된다.
	const auto readRes = ReadArchive(ar);
	return static_cast<bool>(readRes);
}

EResult ActionTimelineAsset::OnPostLoad()
{
	// 이전 파일의 트랙 종류를 한 번 판정한다. 이후 저장하면 판정값이 기록되어 이름을 바꿔도 종류가 유지된다.
	for (auto& track : m_NotifyTracks) track.Kind = ResolveTrackKind(track);
	return EResult::Success;
}

END