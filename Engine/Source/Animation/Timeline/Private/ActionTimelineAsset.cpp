#include "ActionTimelineAsset.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <algorithm>
#include <cmath>
#include <system_error>

BEGIN(Engine)

namespace
{
	std::string NormalizeModelPath(const std::string& path)
	{
		if (path.empty()) return {};
		if (PlatformPath::IsVirtualUri(path)) return path;

		const auto inputPath = Utf8ToPath(path);
		const auto physicalPath = PlatformPath::ResolveUri(inputPath);
		std::error_code error;
		const bool exists = std::filesystem::exists(physicalPath, error) && !error;
		const auto virtualUri = PlatformPath::ToVirtualUri(exists ? physicalPath : inputPath);
		return PlatformPath::IsVirtualUri(virtualUri) ? WStrToStr(virtualUri) : path;
	}
}

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
			m_ModelPath = NormalizeModelPath(desc->ModelPath);
			m_ClipName = desc->ClipName;
			m_Duration = desc->Duration;
			m_EndPolicy = desc->EndPolicy;
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

#if BAM_EDITOR
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
#endif

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

float ActionTimelineAsset::ComputeLastTrackEnd(const std::vector<TimelineNotifyTrack>& notifyTracks, const std::vector<TimelineCameraTrack>& cameraTracks,
	const std::vector<TimelineCameraCutSection>& cutTracks, const std::vector<TimelineCameraSplineSection>& splineTracks)
{
	float lastEnd = 0.0f;
	const auto extend = [&lastEnd](float end) { if (std::isfinite(end) && end > lastEnd) lastEnd = end; };
	for (const auto& track : notifyTracks)
		for (const auto& notify : track.Notifies)
			extend(notify.TriggerTime + std::max(0.0f, notify.Duration));
	for (const auto& shot : cameraTracks) extend(shot.StartTime + std::max(0.0f, shot.Duration));
	for (const auto& section : cutTracks) extend(std::max(section.StartTime, section.EndTime));
	for (const auto& section : splineTracks) extend(std::max(section.StartTime, section.EndTime));
	return lastEnd;
}

float ActionTimelineAsset::ResolvePlaybackDuration(ETimelineEndPolicy policy, float fixedDuration, float clipLength, float lastTrackEnd)
{
	const auto positive = [](float value) { return std::isfinite(value) && value > 0.0f ? value : 0.0f; };
	float duration = positive(fixedDuration);
	if (policy == ETimelineEndPolicy::ClipLength && positive(clipLength) > 0.0f)
		duration = positive(clipLength);
	else if (policy == ETimelineEndPolicy::LastTrackEnd)
		duration = std::max(positive(clipLength), positive(lastTrackEnd));
	return std::max(MinPlaybackDuration, duration);
}

float ActionTimelineAsset::ResolvePlaybackDuration(float clipLength) const
{
	const float lastTrackEnd = m_EndPolicy == ETimelineEndPolicy::LastTrackEnd
		? ComputeLastTrackEnd(m_NotifyTracks, m_CameraTracks, m_CameraCutTracks, m_CameraSplineTracks) : 0.0f;
	return ResolvePlaybackDuration(m_EndPolicy, m_Duration, clipLength, lastTrackEnd);
}

void ActionTimelineAsset::SetModelPath(const std::string& path)
{
	m_ModelPath = NormalizeModelPath(path);
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
	if (readRes) m_ModelPath = NormalizeModelPath(m_ModelPath);
	return static_cast<bool>(readRes);
}

EResult ActionTimelineAsset::OnPostLoad()
{
	// 이전 파일의 트랙 종류를 한 번 판정한다. 이후 저장하면 판정값이 기록되어 이름을 바꿔도 종류가 유지된다.
	for (auto& track : m_NotifyTracks) track.Kind = ResolveTrackKind(track);
	m_ModelPath = NormalizeModelPath(m_ModelPath);
	return EResult::Success;
}

END