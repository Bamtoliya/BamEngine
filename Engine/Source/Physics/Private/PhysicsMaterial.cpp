#include "PhysicsMaterial.h"
#include "PlatformPath.h"
#include "Functions.h"
#include <archive/JsonArchive.h>
#include <system_error>

BEGIN(Engine)

namespace
{
	std::filesystem::path ResolvePhysicsMaterialPath(const std::filesystem::path& path)
	{
		std::error_code ec;
		const auto uri = PlatformPath::ResolveUri(path);
		if (std::filesystem::exists(uri, ec)) return uri;
		return PlatformPath::ResolveProjectRelative(path);
	}
}

EResult PhysicsMaterial::Initialize(void* arg)
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

PhysicsMaterial* PhysicsMaterial::Create(void* arg)
{
	auto* material = new PhysicsMaterial();
	if (IsFailure(material->Initialize(arg)))
	{
		delete material;
		return nullptr;
	}
	return material;
}

bool PhysicsMaterial::SaveToFile(const std::filesystem::path& path) const
{
	const auto fullPath = ResolvePhysicsMaterialPath(path);
	std::error_code ec;
	if (fullPath.has_parent_path()) std::filesystem::create_directories(fullPath.parent_path(), ec);

	archive::JsonArchive ar;
	if (!WriteArchive(ar)) return false;
	return static_cast<bool>(ar.SaveToFile(fullPath));
}

bool PhysicsMaterial::LoadFromFile(const std::filesystem::path& path)
{
	archive::JsonArchive ar;
	if (!ar.LoadFromFile(ResolvePhysicsMaterialPath(path))) return false;
	const std::wstring identityKey = m_Key;
	const std::wstring identityPath = m_Path;
	const bool read = static_cast<bool>(ReadArchive(ar));
	m_Key = identityKey;
	m_Path = identityPath;
	return read;
}

bool PhysicsMaterial::IsPhysicsMaterialFile(const std::filesystem::path& path)
{
	const std::string name = ToLowerStr(WStrToStr(path.filename().wstring()));
	return name.ends_with(".bamphysmat.json") || name.ends_with(".bamphysmat");
}

END
