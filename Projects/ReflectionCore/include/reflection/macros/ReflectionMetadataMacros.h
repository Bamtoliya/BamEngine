#pragma once

#include "reflection/core/Metadata.h"

#define NAME(text) { reflection::CompileTimeHash("Name"), reflection::MakeMetadataValue(text) },
#define TOOLTIP(text) { reflection::CompileTimeHash("Tooltip"), reflection::MakeMetadataValue(text) },
#define CATEGORY(text) { reflection::CompileTimeHash("Category"), reflection::MakeMetadataValue(text) },

#define EDITABLE { reflection::CompileTimeHash("Editable"), true },
#define READONLY { reflection::CompileTimeHash("ReadOnly"), true },
#define TRANSIENT { reflection::CompileTimeHash("Transient"), true },
#define NOSERIALIZE { reflection::CompileTimeHash("NoSerialize"), true },

#define DEFAULT(value) { reflection::CompileTimeHash("Default"), std::string_view(#value) },
#define RANGE(...) { reflection::CompileTimeHash("Range"), std::string_view(#__VA_ARGS__) },
#define COLOR(...) { reflection::CompileTimeHash("Color"), std::string_view(#__VA_ARGS__) },
#define EDITCONDITION(...) { reflection::CompileTimeHash("EditCondition"), std::string_view(#__VA_ARGS__) },
#define ONCHANGED(...) { reflection::CompileTimeHash("OnChanged"), std::string_view(#__VA_ARGS__) },