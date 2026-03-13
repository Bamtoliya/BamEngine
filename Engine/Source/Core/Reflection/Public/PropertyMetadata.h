#pragma once
#include "PropertyAttributes.h"
#include "ReflectionHelper.h"

BEGIN(Engine)

using MetaValue = std::variant<
	string_view,
	wstring_view,
	bool,
	MetaRange,
	MetaColor,
	MetaEditCondition,
	MetaOnChanged,
	span<const pair<string_view, uint32>>,
	int32,
	int64,
	uint32,
	uint64,
	f32,
	f64,
	glm::vec2,
	glm::vec3,
	glm::vec4,
	glm::quat,
	glm::mat3,
	glm::mat4
>;

struct MetadataEntry
{
	uint64 KeyHash;
	MetaValue Value;

	constexpr MetadataEntry(const char* _value) : KeyHash(CompileTimeHash("Name")), Value(string_view(_value)) {}
	constexpr MetadataEntry(const wchar* _value) : KeyHash(CompileTimeHash(L"Name")), Value(wstring_view(_value)) {}
	constexpr MetadataEntry(string_view _value) : KeyHash(CompileTimeHash("Name")), Value(_value) {}
	constexpr MetadataEntry(wstring_view _value) : KeyHash(CompileTimeHash(L"Name")), Value(_value) {}

	constexpr MetadataEntry(uint64 _keyHash, const char* _value) : KeyHash(_keyHash), Value(string_view(_value)) {}
	constexpr MetadataEntry(uint64 _keyHash, const wchar* _value) : KeyHash(_keyHash), Value(wstring_view(_value)) {}
	constexpr MetadataEntry(uint64 _keyHash, string_view _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, wstring_view _value) : KeyHash(_keyHash), Value(_value) {}

	constexpr MetadataEntry(uint64 _keyHash, bool _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, const MetaRange& _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, const MetaColor& _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, const MetaEditCondition& _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, const MetaOnChanged& _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, std::span<const std::pair<std::string_view, uint32>> _value): KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, int32 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, int64 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, uint32 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, uint64 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, f32 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, f64 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, glm::vec2 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, glm::vec3 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, glm::vec4 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, glm::quat _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, glm::mat3 _value) : KeyHash(_keyHash), Value(_value) {}
	constexpr MetadataEntry(uint64 _keyHash, glm::mat4 _value) : KeyHash(_keyHash), Value(_value) {}

	static constexpr const MetadataEntry* Find(std::span<const MetadataEntry> entries, uint64 keyHash)
	{
		for (auto& e : entries)
			if (e.KeyHash == keyHash) return &e;
		return nullptr;
	}
};

END