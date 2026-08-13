#pragma once

#include "Types.h"

#include <reflection/core/QualifiedName.h>

namespace Engine
{
	inline constexpr uint64 MetaNameHash = reflection::CompileTimeHash("Name");
	inline constexpr uint64 MetaTooltipHash = reflection::CompileTimeHash("Tooltip");
	inline constexpr uint64 MetaCategoryHash = reflection::CompileTimeHash("Category");
	inline constexpr uint64 MetaRangeHash = reflection::CompileTimeHash("Range");
	inline constexpr uint64 MetaColorHash = reflection::CompileTimeHash("Color");
	inline constexpr uint64 MetaEditableHash = reflection::CompileTimeHash("Editable");
	inline constexpr uint64 MetaReadOnlyHash = reflection::CompileTimeHash("ReadOnly");
	inline constexpr uint64 MetaFilePathHash = reflection::CompileTimeHash("FilePath");
	inline constexpr uint64 MetaDirectoryHash = reflection::CompileTimeHash("Directory");
	inline constexpr uint64 MetaEditConditionHash = reflection::CompileTimeHash("EditCondition");
	inline constexpr uint64 MetaDefaultHash = reflection::CompileTimeHash("Default");
	inline constexpr uint64 MetaOnChangedHash = reflection::CompileTimeHash("OnChanged");
	inline constexpr uint64 MetaNoSerializeHash = reflection::CompileTimeHash("NoSerialize");
}