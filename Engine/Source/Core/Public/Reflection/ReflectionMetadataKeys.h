#pragma once

#include "Types.h"

#include <reflection/core/QualifiedName.h>

namespace Engine
{
	inline constexpr uint64 MetaNameHash = legacy_reflection::CompileTimeHash("Name");
	inline constexpr uint64 MetaTooltipHash = legacy_reflection::CompileTimeHash("Tooltip");
	inline constexpr uint64 MetaCategoryHash = legacy_reflection::CompileTimeHash("Category");
	inline constexpr uint64 MetaRangeHash = legacy_reflection::CompileTimeHash("Range");
	inline constexpr uint64 MetaColorHash = legacy_reflection::CompileTimeHash("Color");
	inline constexpr uint64 MetaEditableHash = legacy_reflection::CompileTimeHash("Editable");
	inline constexpr uint64 MetaReadOnlyHash = legacy_reflection::CompileTimeHash("ReadOnly");
	inline constexpr uint64 MetaFilePathHash = legacy_reflection::CompileTimeHash("FilePath");
	inline constexpr uint64 MetaDirectoryHash = legacy_reflection::CompileTimeHash("Directory");
	inline constexpr uint64 MetaEditConditionHash = legacy_reflection::CompileTimeHash("EditCondition");
	inline constexpr uint64 MetaDefaultHash = legacy_reflection::CompileTimeHash("Default");
	inline constexpr uint64 MetaOnChangedHash = legacy_reflection::CompileTimeHash("OnChanged");
	inline constexpr uint64 MetaNoSerializeHash = legacy_reflection::CompileTimeHash("NoSerialize");
}