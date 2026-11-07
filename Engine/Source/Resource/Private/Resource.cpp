#include "Resource.h"
#include "ReflectionRegistry.h"
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include "Logger.h"
#include "PlatformPath.h"

namespace
{
	constexpr uint64 LegacyRoundedAssetMagic = 6072351289621955000ULL;

	bool IsValidAssetMagic(uint64 magic) noexcept
	{
		return magic == ENGINE_ASSET_MAGIC || magic == LegacyRoundedAssetMagic;
	}
}

archive::ArchiveResult Resource::WriteArchive(archive::IArchive& ar) const
{
	if (!ReflectionRegistry::IsCreated())
		return { archive::ArchiveErrorCode::InvalidOperation, "Resource.ReflectionRegistry" };

	const auto& reflection = ReflectionRegistry::Get();
	const auto view = GetReflectedObjectView();
	const auto* type = reflection.GetRegistry().FindType(view.GetCppType());
	if (!view.IsValid() || type == nullptr)
		return { archive::ArchiveErrorCode::UnsupportedType, "Resource.Type" };

	ResourceBinaryHeader assetHeader;
	assetHeader.MagicNumber = ENGINE_ASSET_MAGIC;
	assetHeader.ResourceType = m_ResourceType;
	assetHeader.Version = GetArchiveFormatVersion();

	auto result = ar.WriteObject("AssetHeader", assetHeader,
		[](archive::IArchive& fields, const ResourceBinaryHeader& header) -> archive::ArchiveResult
		{
			auto r = fields.Write("MagicNumber", header.MagicNumber);
			if (r) r = fields.Write("ResourceType", header.ResourceType);
			if (r) r = fields.Write("Version", header.Version);
			return r;
		});
	if (!result)
		return result;

	return archive_reflection::detail::WriteProperties(
		ar, *type, view, &reflection.GetRegistry(), &reflection.GetPropertyCodecs());
}

archive::ArchiveResult Resource::ReadArchive(const archive::IArchive& ar)
{
	if (!ReflectionRegistry::IsCreated())
		return { archive::ArchiveErrorCode::InvalidOperation, "Resource.ReflectionRegistry" };

	const auto& reflection = ReflectionRegistry::Get();
	const auto view = GetReflectedObjectView();
	const auto* type = reflection.GetRegistry().FindType(view.GetCppType());
	if (!view.IsValid() || type == nullptr)
		return { archive::ArchiveErrorCode::UnsupportedType, "Resource.Type" };

	ResourceBinaryHeader assetHeader;
	auto result = ar.ReadObject("AssetHeader", assetHeader,
		[](archive::IArchive& fields, ResourceBinaryHeader& header) -> archive::ArchiveResult
		{
			auto r = fields.Read("MagicNumber", header.MagicNumber);
			if (!r)
			{
				std::string magicStr;
				if (fields.Read("MagicNumber", magicStr) && magicStr == std::to_string(ENGINE_ASSET_MAGIC))
				{
					header.MagicNumber = ENGINE_ASSET_MAGIC;
					r = {};
				}
				else
				{
					return r;
				}
			}
			if (r) r = fields.Read("ResourceType", header.ResourceType);
			if (r) r = fields.Read("Version", header.Version);
			return r;
		});
	if (!result)
		return result;

	if (!IsValidAssetMagic(assetHeader.MagicNumber))
		return { archive::ArchiveErrorCode::InvalidDocument, "Resource.AssetHeader.MagicNumber" };

	if (assetHeader.ResourceType != m_ResourceType)
		return { archive::ArchiveErrorCode::InvalidValue, "Resource.AssetHeader.ResourceType" };

	if (assetHeader.Version != GetArchiveFormatVersion())
		return { archive::ArchiveErrorCode::UnsupportedType, "Resource.AssetHeader.Version" };

	auto& mutableAr = const_cast<archive::IArchive&>(ar);
	const auto savedType = m_ResourceType;
	result = archive_reflection::detail::VisitProperties(*type, view, &reflection.GetRegistry(),
		[&](const reflection::PropertyInfo& property, const reflection::ObjectView& owner) -> archive::ArchiveResult
		{
			bool hasField = false;
			auto fieldCheck = ar.HasField(property.Name, hasField);
			if (!fieldCheck)
				return fieldCheck;
			if (!hasField)
				return {};
			return archive_reflection::detail::ReadProperty(
				mutableAr, property, owner, &reflection.GetRegistry(), &reflection.GetPropertyCodecs());
		});
	if (!result)
		return result;

	m_ResourceType = savedType;
	if (!m_Key.empty())
		m_Key = NormalizePath(m_Key);
	if (!m_Path.empty())
		m_Path = NormalizePath(m_Path);
	if (OnPostLoad() != EResult::Success)
		return { archive::ArchiveErrorCode::InvalidValue, "Resource.OnPostLoad" };
	return {};
}
EResult Resource::Reload(const std::filesystem::path& filePath)
{
	const auto& storedPath = m_Path.empty() ? m_Key : m_Path;
	auto targetPath = filePath.empty() ? std::filesystem::path(storedPath) : filePath;

	std::error_code ec;
	if (!std::filesystem::exists(targetPath, ec))
	{
		if (targetPath.is_relative())
		{
			const auto resolved = PlatformPath::ResolveProjectRelative(targetPath);
			if (std::filesystem::exists(resolved, ec))
				targetPath = resolved;
			else
				return EResult::FileNotFound;
		}
		else
		{
			return EResult::FileNotFound;
		}
	}

	auto extension = targetPath.extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	const auto reloadArchive = [&](archive::IArchive& ar) -> EResult
		{
			const char* stage = "load archive";
			auto result = ar.LoadFromFile(targetPath);
			if (result)
			{
				stage = "deserialize resource";
				result = ReadArchive(ar);
			}

			const auto path = WStrToStr(targetPath.wstring());
			if (!result)
			{
				ENGINE_LOG_ERROR("[Resource Hot Reload] Failed: stage={}, path={}, code={}, field={}",
					stage, path, static_cast<int>(result.Code), result.Field);
				return EResult::Fail;
			}

			IncreaseVersion();
			ENGINE_LOG_INFO("[Resource Hot Reload] Successfully reloaded: {}", path);
			return EResult::Success;
		};

	if (extension == L".json")
	{
		archive::JsonArchive ar;
		return reloadArchive(ar);
	}

	archive::BeveArchive ar;
	return reloadArchive(ar);
}