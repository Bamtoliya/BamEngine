#pragma once

#include "ReflectionRegistry.h"

#define NAME(text) {Engine::CompileTimeHash("Name"), text},
#define TOOLTIP(text) {Engine::CompileTimeHash("Tooltip"), text},
#define CATEGORY(text) {Engine::CompileTimeHash("Category"), text},
#define RANGE(...) {Engine::CompileTimeHash("Range"), Engine::MetaRange{ __VA_ARGS__ }},
#define COLOR(r, g, b, a) {Engine::CompileTimeHash("Color"), Engine::MetaColor{ vec4(r, g, b, a) }},
#define EDITABLE {Engine::CompileTimeHash("Editable"), true},
#define NOSERIALIZE {Engine::CompileTimeHash("NoSerialize"), true},
#define READONLY {Engine::CompileTimeHash("ReadOnly"), true},
#define FILEPATH(filter) {Engine::CompileTimeHash("FilePath"), filter},
#define DIRECTORY {Engine::CompileTimeHash("Directory"), true},
#define EDITCONDITION(conditionVar, ...) {Engine::CompileTimeHash("EditCondition"), Engine::MetaEditCondition{ conditionVar, __VA_ARGS__ }},
#define DEFAULT(value) {Engine::CompileTimeHash("Default"), std::string_view(#value)},
#define ONCHANGED(...) {Engine::CompileTimeHash("OnChanged"), Engine::MetaOnChanged{ __VA_ARGS__ }},