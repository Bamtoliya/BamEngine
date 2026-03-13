#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)

inline constexpr uint64 MetaNameHash = Engine::CompileTimeHash("Name");
inline constexpr uint64 MetaTooltipHash = Engine::CompileTimeHash("Tooltip");
inline constexpr uint64 MetaCategoryHash = Engine::CompileTimeHash("Category");
inline constexpr uint64 MetaRangeHash = Engine::CompileTimeHash("Range");
inline constexpr uint64 MetaColorHash = Engine::CompileTimeHash("Color");
inline constexpr uint64 MetaReadOnlyHash = Engine::CompileTimeHash("ReadOnly");
inline constexpr uint64 MetaFilePathHash = Engine::CompileTimeHash("FilePath");
inline constexpr uint64 MetaDirectoryHash = Engine::CompileTimeHash("Directory");
inline constexpr uint64 MetaEditConditionHash = Engine::CompileTimeHash("EditCondition");


#pragma region MetadataEntry

inline const Engine::MetadataEntry* FindMetadata(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash)
{
	return Engine::MetadataEntry::Find(metadata, keyHash);
}

inline string GetMetadataString(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash, const string& fallback = "")
{
	const auto* entry = FindMetadata(metadata, keyHash);
	if (!entry) return fallback;

	const auto* value = std::get_if<string_view>(&entry->Value);
	if (value) return string(*value);

	const auto* wvalue = std::get_if<wstring_view>(&entry->Value);
	if (wvalue) return Engine::WStrToStr(wvalue->data());

	return fallback;
}

inline string GetMetadataName(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
	return GetMetadataString(metadata, MetaNameHash, fallback);
}

inline string GetMetadataTooltip(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
	return GetMetadataString(metadata, MetaTooltipHash, fallback);
}

inline string GetMetadataCategory(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
	return GetMetadataString(metadata, MetaCategoryHash, fallback);
}

inline string GetMetadataFilePath(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
	return GetMetadataString(metadata, MetaFilePathHash, fallback);
}

inline string GetMetadataDirectory(std::span<const Engine::MetadataEntry> metadata, const string& fallback = "")
{
	return GetMetadataString(metadata, MetaDirectoryHash, fallback);
}

inline bool GetMetadataBool(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash, bool fallback = false)
{
	const auto* entry = FindMetadata(metadata, keyHash);
	if (!entry) return fallback;
	const auto* value = std::get_if<bool>(&entry->Value);
	if (value) return *value;
	return fallback;
}

inline MetaRange* GetMetadataRange(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash)
{
	const auto* entry = FindMetadata(metadata, keyHash);
	if (!entry) return nullptr;
	const auto* value = std::get_if<MetaRange>(&entry->Value);
	if (value) return const_cast<MetaRange*>(value);
	return nullptr;
}

inline MetaColor* GetMetadataColor(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash)
{
	const auto* entry = FindMetadata(metadata, keyHash);
	if (!entry) return nullptr;
	const auto* value = std::get_if<MetaColor>(&entry->Value);
	if (value) return const_cast<MetaColor*>(value);
	return nullptr;
}

inline MetaEditCondition* GetMetadataEditCondition(std::span<const Engine::MetadataEntry> metadata, uint64 keyHash)
{
	const auto* entry = FindMetadata(metadata, keyHash);
	if (!entry) return nullptr;
	const auto* value = std::get_if<MetaEditCondition>(&entry->Value);
	if (value) return const_cast<MetaEditCondition*>(value);
	return nullptr;
}

inline bool GetMetadataReadOnly(std::span<const Engine::MetadataEntry> metadata)
{
	return GetMetadataBool(metadata, MetaReadOnlyHash, false);
}

inline string NormalizeReflectedTypeName(string_view rawTypeName)
{
	string name(rawTypeName);
	if (name.starts_with("class "))
	{
		name.erase(0, 6);
	}
	else if (name.starts_with("struct "))
	{
		name.erase(0, 7);
	}

	while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
	{
		name.erase(name.begin());
	}

	while (!name.empty())
	{
		const char last = name.back();
		if (last == '*' || last == '&' || std::isspace(static_cast<unsigned char>(last)))
		{
			name.pop_back();
			continue;
		}
		break;
	}
	return name;
}

inline const Engine::PropertyInfo* FindPropertyByName(const Engine::TypeInfo& typeInfo, string_view propertyName)
{
	const Engine::TypeInfo* current = &typeInfo;

	while (current != nullptr)
	{
		for (const auto& prop : current->Properties)
		{
			if (prop.Name == propertyName)
			{
				return &prop;
			}
		}

		if (current->ParentName.empty())
		{
			break;
		}

		current = Engine::ReflectionRegistry::Get().GetType(string(current->ParentName));
	}

	return nullptr;
}

#pragma endregion


#pragma region Helper
// Enum이나 정수형 데이터를 안전하게 읽어오는 헬퍼
inline int64 ReadInteger(void* data, size_t size)

{
	switch (size)
	{
	case 1: return *(int8_t*)data;
	case 2: return *(int16_t*)data;
	case 4: return *(int32_t*)data;
	case 8: return *(int64_t*)data;
	default: return 0;
	}
}

// 데이터를 다시 원본 메모리에 쓰는 헬퍼
inline void WriteInteger(void* data, size_t size, int64_t value)
{
	switch (size)
	{
	case 1: *(int8_t*)data = (int8_t)value; break;
	case 2: *(int16_t*)data = (int16_t)value; break;
	case 4: *(int32_t*)data = (int32_t)value; break;
	case 8: *(int64_t*)data = (int64_t)value; break;
	}
}

inline bool CheckEditCondition(void* instance, const TypeInfo& typeInfo, std::span<const Engine::MetadataEntry> metadata)
{
	(void)instance;
	(void)typeInfo;
	(void)metadata;

	// 새 메타데이터 구조 대응 우선.
	// EditCondition 파서가 확정되기 전까지는 표시만 유지.
	return true;
}
#pragma endregion
END