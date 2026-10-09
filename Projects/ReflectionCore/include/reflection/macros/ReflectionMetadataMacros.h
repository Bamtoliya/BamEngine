#pragma once

#include "reflection/core/Metadata.h"

#define DISPLAY_NAME(text) { legacy_reflection::CompileTimeHash("Name"), legacy_reflection::MakeMetadataValue(text) },
#define TOOLTIP(text) { legacy_reflection::CompileTimeHash("Tooltip"), legacy_reflection::MakeMetadataValue(text) },
#define CATEGORY(text) { legacy_reflection::CompileTimeHash("Category"), legacy_reflection::MakeMetadataValue(text) },

#define EDITABLE { legacy_reflection::CompileTimeHash("Editable"), true },
#define READONLY { legacy_reflection::CompileTimeHash("ReadOnly"), true },
#define TRANSIENT { legacy_reflection::CompileTimeHash("Transient"), true },
#define NOSERIALIZE { legacy_reflection::CompileTimeHash("NoSerialize"), true },

#define DEFAULT(value) { legacy_reflection::CompileTimeHash("Default"), std::string_view(#value) },
#define RANGE(...) { legacy_reflection::CompileTimeHash("Range"), std::string_view(#__VA_ARGS__) },
#define COLOR { legacy_reflection::CompileTimeHash("Color"), std::string_view{} },
#define EDITCONDITION(...) { legacy_reflection::CompileTimeHash("EditCondition"), std::string_view(#__VA_ARGS__) },
#define ONCHANGED(...) { legacy_reflection::CompileTimeHash("OnChanged"), std::string_view(#__VA_ARGS__) },