#include "TerrainDataFile.h"
#include "TerrainComponent.h"
#include "TerrainUtility.h"
#include "IDComponent.h"
#include "PakArchive.h"
#include "PlatformPath.h"
#include "Functions.h"
#include "Logger.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <system_error>

BEGIN(Engine)

namespace
{
	constexpr std::array<char, 8> Magic{ 'B', 'A', 'M', 'T', 'E', 'R', 'R', '\0' };
	constexpr uint32 HeightFormatR16 = 1;
	constexpr uint32 SplatChannelsV1 = 4; // v1: 레이어 0~3만, 속성 맵 없음
	constexpr uint32 SplatChannelsV2 = TerrainComponent::MaxLayers; // v2: 레이어 0~7 + 속성 맵

	struct Header
	{
		std::array<char, 8> Magic{};
		uint32 Version = 0;
		uint32 Resolution = 0;
		uint32 HeightFormat = 0;
		uint32 SplatChannels = 0;
	};
	static_assert(sizeof(Header) == 24, "Terrain data header layout must not change.");

	std::filesystem::path ResolveDataPath(const std::string& dataPath)
	{
		const auto path = Utf8ToPath(dataPath);
		return PlatformPath::IsVirtualUri(dataPath) ? PlatformPath::ResolveUri(path) : PlatformPath::ResolveProjectRelative(path);
	}

	std::wstring SceneBaseName(const std::filesystem::path& scenePath)
	{
		auto name = scenePath.filename().wstring();
		for (const auto* suffix : { L".bamscene.json", L".json", L".bamscene" })
		{
			const std::wstring_view view(suffix);
			if (name.size() > view.size() && name.ends_with(view))
			{
				name.resize(name.size() - view.size());
				break;
			}
		}
		return name;
	}

	TerrainDataContent ContentOf(const TerrainComponent& terrain)
	{
		return { terrain.Resolution, terrain.HeightData, terrain.SplatData, terrain.SplatDataExtra, terrain.AttributeData };
	}

	void ApplyContent(TerrainComponent& terrain, TerrainDataContent&& content)
	{
		terrain.HeightData = std::move(content.Heights);
		terrain.SplatData = std::move(content.Splat);
		terrain.SplatDataExtra = std::move(content.SplatExtra);
		terrain.AttributeData = std::move(content.Attributes);
		terrain.MarkHeightDirty();
		terrain.IsSplatDirty = true;
		terrain.IsAttributeDirty = true;
	}
}

bool TerrainDataFile::Write(const std::filesystem::path& path, const TerrainDataContent& content)
{
	uint32 axisCount = 0;
	std::size_t sampleCount = 0;
	if (path.empty() || !TerrainUtility::TryGetGridLayout(content.Resolution, axisCount, sampleCount) ||
		content.Heights.size() != sampleCount || content.Splat.size() != sampleCount * 4 ||
		(!content.SplatExtra.empty() && content.SplatExtra.size() != sampleCount * 4) ||
		(!content.Attributes.empty() && content.Attributes.size() != sampleCount))
		return false;

	std::vector<uint16> quantized(sampleCount);
	for (std::size_t index = 0; index < sampleCount; ++index)
	{
		const f32 height = std::isfinite(content.Heights[index]) ? std::clamp(content.Heights[index], 0.0f, 1.0f) : 0.0f;
		quantized[index] = static_cast<uint16>(std::lround(height * 65535.0f));
	}
	// 비어 있는 레이어 4~7·속성 맵은 0으로 쓴다.
	const std::vector<uint8> zeroExtra = content.SplatExtra.empty() ? std::vector<uint8>(sampleCount * 4, 0) : std::vector<uint8>{};
	const std::vector<uint8> zeroAttributes = content.Attributes.empty() ? std::vector<uint8>(sampleCount, 0) : std::vector<uint8>{};
	const auto& extra = content.SplatExtra.empty() ? zeroExtra : content.SplatExtra;
	const auto& attributes = content.Attributes.empty() ? zeroAttributes : content.Attributes;

	std::error_code ec;
	if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
	auto tempPath = path;
	tempPath += L".tmp";
	{
		std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
		if (!file) return false;
		const Header header{ Magic, Version, content.Resolution, HeightFormatR16, SplatChannelsV2 };
		file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		file.write(reinterpret_cast<const char*>(quantized.data()), static_cast<std::streamsize>(quantized.size() * sizeof(uint16)));
		file.write(reinterpret_cast<const char*>(content.Splat.data()), static_cast<std::streamsize>(content.Splat.size()));
		file.write(reinterpret_cast<const char*>(extra.data()), static_cast<std::streamsize>(extra.size()));
		file.write(reinterpret_cast<const char*>(attributes.data()), static_cast<std::streamsize>(attributes.size()));
		if (!file.flush())
		{
			file.close();
			std::filesystem::remove(tempPath, ec);
			return false;
		}
	}
	std::filesystem::rename(tempPath, path, ec);
	if (ec)
	{
		std::filesystem::remove(tempPath, ec);
		return false;
	}
	return true;
}

bool TerrainDataFile::Read(const std::filesystem::path& path, TerrainDataContent& out)
{
	std::vector<uint8_t> bytes;
	if (PakMountManager::Get().ReadBytesWithFallback(path, bytes) != EResult::Success) return false;
	const uint64 fileSize = static_cast<uint64>(bytes.size());

	Header header;
	if (fileSize < sizeof(Header)) return false;
	std::memcpy(&header, bytes.data(), sizeof(Header));
	if (header.Magic != Magic || header.HeightFormat != HeightFormatR16) return false;

	const bool isV1 = header.Version == 1 && header.SplatChannels == SplatChannelsV1;
	const bool isV2 = header.Version == 2 && header.SplatChannels == SplatChannelsV2;
	if (!isV1 && !isV2) return false;

	uint32 axisCount = 0;
	std::size_t sampleCount = 0;
	if (!TerrainUtility::TryGetGridLayout(header.Resolution, axisCount, sampleCount) ||
		fileSize != sizeof(Header) + sampleCount * sizeof(uint16) + sampleCount * header.SplatChannels + (isV2 ? sampleCount : 0))
		return false;

	size_t offset = sizeof(Header);
	std::vector<uint16> quantized(sampleCount);
	const size_t heightBytes = quantized.size() * sizeof(uint16);
	std::memcpy(quantized.data(), bytes.data() + offset, heightBytes);
	offset += heightBytes;

	TerrainDataContent content;
	content.Resolution = header.Resolution;
	content.Splat.resize(sampleCount * 4);
	content.SplatExtra.assign(sampleCount * 4, 0);
	content.Attributes.assign(sampleCount, 0);

	std::memcpy(content.Splat.data(), bytes.data() + offset, content.Splat.size());
	offset += content.Splat.size();
	if (isV2)
	{
		std::memcpy(content.SplatExtra.data(), bytes.data() + offset, content.SplatExtra.size());
		offset += content.SplatExtra.size();
		std::memcpy(content.Attributes.data(), bytes.data() + offset, content.Attributes.size());
	}

	content.Heights.resize(sampleCount);
	for (std::size_t index = 0; index < sampleCount; ++index)
		content.Heights[index] = static_cast<f32>(quantized[index]) / 65535.0f;
	out = std::move(content);
	return true;
}

std::filesystem::path TerrainDataFile::MakeDefaultPath(const std::filesystem::path& scenePath, uint64 entityId)
{
	wchar_t idText[17]{};
	std::swprintf(idText, std::size(idText), L"%016llX", static_cast<unsigned long long>(entityId));
	return scenePath.parent_path() / (SceneBaseName(scenePath) + L"_Terrain_" + idText + L".bamterrain");
}

bool TerrainDataFile::SaveSceneTerrains(entt::registry& registry, const std::filesystem::path& scenePath)
{
	bool succeeded = true;
	for (auto [entity, terrain] : registry.view<TerrainComponent>().each())
	{
		// 아직 데이터가 만들어지지 않은 지형은 저장할 내용이 없다(처음 갱신 때 기본 형태가 생성된다).
		// IsDataDirty만 보고 건너뛰지 않는다. Undo가 컴포넌트 사본을 되돌리면 표시도 편집 전 값으로 돌아가 변경을 놓칠 수 있다.
		if (terrain.HeightData.empty()) continue;

		std::string dataPath = terrain.DataPath;
		if (dataPath.empty())
		{
			const auto* id = registry.try_get<IDComponent>(entity);
			const uint64 entityId = id && id->id != 0 ? id->id : static_cast<uint64>(entt::to_integral(entity));
			dataPath = Engine::Unicode::WideToUtf8(PlatformPath::ToVirtualUri(MakeDefaultPath(scenePath, entityId)));
		}

		if (!Write(ResolveDataPath(dataPath), ContentOf(terrain)))
		{
			BAM_LOG(Error, "Engine", "Terrain", "Terrain data save failed: {}", dataPath);
			succeeded = false;
			continue;
		}
		terrain.DataPath = dataPath;
		terrain.LoadedDataPath = dataPath; // 방금 쓴 내용이 메모리와 같으므로 다시 읽지 않는다.
		terrain.IsDataDirty = false;
	}
	return succeeded;
}

void TerrainDataFile::LoadIfChanged(TerrainComponent& terrain)
{
	// 복구한 씬: 임시 사본을 읽고, 원본 파일과 다르므로 편집된 상태로 둔다. 원본 DataPath는 다시 읽지 않는다.
	if (!terrain.RecoveryDataPath.empty())
	{
		const auto recoveryPath = Utf8ToPath(terrain.RecoveryDataPath);
		terrain.RecoveryDataPath.clear();
		terrain.LoadedDataPath = terrain.DataPath;
		TerrainDataContent content;
		if (Read(recoveryPath, content))
		{
			ApplyContent(terrain, std::move(content));
			terrain.IsDataDirty = true;
			return;
		}
		BAM_LOG(Warn, "Engine", "Terrain", "Recovered terrain data could not be read; using the saved data file: {}", Engine::Unicode::WideToUtf8(recoveryPath.wstring()));
		terrain.LoadedDataPath.clear(); // 아래에서 원본 파일을 읽는다
	}

	if (terrain.DataPath == terrain.LoadedDataPath) return;
	terrain.LoadedDataPath = terrain.DataPath;
	if (terrain.DataPath.empty()) return;

	TerrainDataContent content;
	if (!Read(ResolveDataPath(terrain.DataPath), content))
	{
		BAM_LOG(Warn, "Engine", "Terrain", "Terrain data could not be read; keeping current heights: {}", terrain.DataPath);
		return;
	}
	ApplyContent(terrain, std::move(content));
	terrain.IsDataDirty = false;
}

namespace
{
	uint64 TerrainEntityId(const entt::registry& registry, entt::entity entity)
	{
		const auto* id = registry.try_get<IDComponent>(entity);
		return id ? id->id : 0;
	}
}

TerrainDataFile::MemorySnapshot TerrainDataFile::CaptureMemory(const entt::registry& registry)
{
	MemorySnapshot snapshot;
	for (auto [entity, terrain] : registry.view<TerrainComponent>().each())
	{
		const uint64 id = TerrainEntityId(registry, entity);
		if (id == 0 || terrain.HeightData.empty()) continue;
		snapshot.Entries.push_back({ id, { ContentOf(terrain), terrain.IsDataDirty } });
	}
	return snapshot;
}

void TerrainDataFile::RestoreMemory(entt::registry& registry, MemorySnapshot&& snapshot)
{
	for (auto [entity, terrain] : registry.view<TerrainComponent>().each())
	{
		const uint64 id = TerrainEntityId(registry, entity);
		if (id == 0) continue;
		const auto found = std::find_if(snapshot.Entries.begin(), snapshot.Entries.end(), [id](const auto& entry) { return entry.first == id; });
		if (found == snapshot.Entries.end()) continue;
		ApplyContent(terrain, std::move(found->second.Content));
		terrain.IsDataDirty = found->second.IsDataDirty;
		terrain.LoadedDataPath = terrain.DataPath; // 메모리 사본이 최신이므로 저장된 파일로 덮어쓰지 않는다
	}
}

TerrainDataFile::ScopedRecoveryCopies::ScopedRecoveryCopies(entt::registry& registry, const std::filesystem::path& directory, const std::wstring& prefix)
	: m_Registry(registry)
{
	for (auto [entity, terrain] : registry.view<TerrainComponent>().each())
	{
		if (terrain.HeightData.empty()) continue;
		const uint64 id = TerrainEntityId(registry, entity);
		const uint64 fileId = id != 0 ? id : static_cast<uint64>(entt::to_integral(entity));
		wchar_t idText[17]{};
		std::swprintf(idText, std::size(idText), L"%016llX", static_cast<unsigned long long>(fileId));
		const auto path = directory / (prefix + L"_Terrain_" + idText + L".bamterrain");
		if (!Write(path, ContentOf(terrain)))
		{
			BAM_LOG(Error, "Engine", "Terrain", "Recovery terrain copy failed: {}", Engine::Unicode::WideToUtf8(path.wstring()));
			m_Succeeded = false;
			continue;
		}
		terrain.RecoveryDataPath = Engine::Unicode::WideToUtf8(path.wstring());
		m_Entities.push_back(entity);
	}
}

TerrainDataFile::ScopedRecoveryCopies::~ScopedRecoveryCopies()
{
	for (const auto entity : m_Entities)
		if (auto* terrain = m_Registry.try_get<TerrainComponent>(entity)) terrain->RecoveryDataPath.clear();
}

END
