#include "TerrainLayerAsset.h"
#include "PlatformPath.h"
#include "Functions.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <system_error>

BEGIN(Engine)

namespace
{
	std::filesystem::path ResolveLayerPath(const std::filesystem::path& path)
	{
		std::error_code ec;
		const auto uri = PlatformPath::ResolveUri(path);
		if (std::filesystem::exists(uri, ec)) return uri;
		return PlatformPath::ResolveProjectRelative(path);
	}
}

EResult TerrainLayerAsset::Initialize(void* arg)
{
	if (IsFailure(Resource::Initialize(arg))) return EResult::Fail;
	if (arg)
	{
		CAST_DESC;
		if (!desc->Path.empty() && !LoadFromFile(std::filesystem::path(desc->Path)))
			return EResult::Fail;
	}
	return EResult::Success;
}

TerrainLayerAsset* TerrainLayerAsset::Create(void* arg)
{
	auto* asset = new TerrainLayerAsset();
	if (IsFailure(asset->Initialize(arg)))
	{
		delete asset;
		return nullptr;
	}
	return asset;
}

bool TerrainLayerAsset::SaveToFile(const std::filesystem::path& path) const
{
	const auto fullPath = ResolveLayerPath(path);
	std::error_code ec;
	if (fullPath.has_parent_path()) std::filesystem::create_directories(fullPath.parent_path(), ec);

	archive::JsonArchive ar;
	if (!WriteArchive(ar)) return false;
	return static_cast<bool>(ar.SaveToFile(fullPath));
}

bool TerrainLayerAsset::LoadFromFile(const std::filesystem::path& path)
{
	archive::JsonArchive ar;
	if (!ar.LoadFromFile(ResolveLayerPath(path))) return false;
	// 키·경로는 ResourceManager 등록과 묶인 식별자라 파일에 적힌 값으로 바꾸지 않는다(Resource::Reload와 같은 규칙).
	const std::wstring identityKey = m_Key;
	const std::wstring identityPath = m_Path;
	const bool read = static_cast<bool>(ReadArchive(ar));
	m_Key = identityKey;
	m_Path = identityPath;
	return read;
}

std::string TerrainLayerAsset::GetDisplayName() const
{
	const auto& stored = GetPath().empty() ? GetKey() : GetPath();
	std::wstring name = std::filesystem::path(stored).filename().wstring();
	for (const auto* suffix : { L".bamterrainlayer.json", L".bamterrainlayer" })
	{
		const std::wstring_view view(suffix);
		if (name.size() > view.size() && name.ends_with(view))
		{
			name.resize(name.size() - view.size());
			break;
		}
	}
	return WStrToStr(name);
}

bool TerrainLayerAsset::IsTerrainLayerFile(const std::filesystem::path& path)
{
	const std::string name = ToLowerStr(WStrToStr(path.filename().wstring()));
	return name.ends_with(".bamterrainlayer.json") || name.ends_with(".bamterrainlayer");
}

END
