#pragma once
#include "ReflectionRegistry.h"

#define PROPERTY(...)
#define CLASS()
#define STRUCT()
#define ENUM(...)
#define FUNCTION(...)


#define REFLECT_CDO \
	static void CreateCDO(void* buffer); \
	static void DestroyCDO(void* buffer); \
	static void CopyCDO(void* dst, const void* src); \
	static void* HeapNew(); \
	virtual void PostLoad() {}


#define REFLECT_BASE() \
public: \
	static const Engine::TypeInfo& GetStaticType(); \
	virtual const Engine::TypeInfo& GetType() const { return GetStaticType(); } \
	static constexpr std::span<const Engine::PropertyInfo> GetProperties() noexcept; \
	REFLECT_CDO

#define REFLECT_STRUCT() REFLECT_BASE()

#define REFLECT_CLASS() \
public: \
	static const Engine::TypeInfo& GetStaticType(); \
	static constexpr std::span<const Engine::PropertyInfo> GetProperties() noexcept; \
	virtual const Engine::TypeInfo& GetType() const override { return GetStaticType(); } \
	REFLECT_CDO

#define BEGIN_METADATA(ClassName, PropName) \
	static constexpr Engine::MetadataEntry ClassName##_##PropName##_Meta[] = {

#define END_METADATA };

#define DECLARE_CONTAINER_INFO(ClassName, PropName, InnerTypeName, InnerTypeEnum, ...) \
	static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
	static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
		{InnerTypeName, InnerTypeEnum}, {"", Engine::EPropertyType::None}, &ClassName##_##PropName##_Accessor, nullptr \
	};

#define DECLARE_NESTED_CONTAINER_INFO(ClassName, PropName, InnerTypeName, InnerTypeEnum, InnerContainerPtr, ...) \
	static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
	static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
		{InnerTypeName, InnerTypeEnum}, {"", Engine::EPropertyType::None}, &ClassName##_##PropName##_Accessor, InnerContainerPtr \
	};

#define DECLARE_MAP_INFO(ClassName, PropName, KeyTypeName, KeyTypeEnum, InnerTypeName, InnerTypeEnum, ...) \
	static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
	static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
		{InnerTypeName, InnerTypeEnum}, {KeyTypeName, KeyTypeEnum}, &ClassName##_##PropName##_Accessor, nullptr \
	};

#define DECLARE_MAP_NESTED_VALUE_INFO(ClassName, PropName, KeyTypeName, KeyTypeEnum, ValueTypeName, ValueTypeEnum, ValueContainerPtr, ...) \
    static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
    static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
        {ValueTypeName, ValueTypeEnum}, {KeyTypeName, KeyTypeEnum}, \
        &ClassName##_##PropName##_Accessor, ValueContainerPtr \
    };

#define BEGIN_PROPERTIES(ClassName) \
	constexpr std::span<const Engine::PropertyInfo> ClassName::GetProperties() noexcept { \
		static constexpr Engine::PropertyInfo Props[] = {

#define END_PROPERTIES \
		}; \
		return Props; \
	}

#define EMPTY_PROPERTIES(ClassName) \
	constexpr std::span<const Engine::PropertyInfo> ClassName::GetProperties() noexcept { \
		return {}; \
	}

#define EMPTY_FUNCTIONS(ClassName) \
	static constexpr std::span<const Engine::FunctionInfo> ClassName##_Functions{};

#define REFLECT_PROPERTY(Class, VarName, TypeName, TypeEnum, MetaArray) \
	{ Engine::CompileTimeHash(#VarName), #VarName, {TypeName, TypeEnum}, offsetof(Class, VarName), sizeof(Class::VarName), nullptr, MetaArray, \
	  &Engine::PropertyCopy<decltype(Class::VarName)>, &Engine::PropertyEqual<decltype(Class::VarName)> },

#define REFLECT_CONTAINER_PROPERTY(Class, VarName, TypeName, TypeEnum, ContainerDataPtr, MetaArray) \
	{ Engine::CompileTimeHash(#VarName), #VarName, {TypeName, TypeEnum}, offsetof(Class, VarName), sizeof(Class::VarName), ContainerDataPtr, MetaArray, \
	  &Engine::PropertyCopy<decltype(Class::VarName)>, &Engine::PropertyEqual<decltype(Class::VarName)> },

#define DECLARE_FUNCTION_PARAMS(ClassName, FuncName) \
	static constexpr Engine::VariableInfo ClassName##_##FuncName##_Params[] = {

#define FUNCTION_PARAM(TypeName, TypeEnum) \
	{TypeName, TypeEnum},

#define END_FUNCTION_PARAMS };

#define BEGIN_FUNCTIONS(ClassName) \
	static constexpr Engine::FunctionInfo ClassName##_Functions[] = {

#define END_FUNCTIONS };

#define REFLECT_FUNCTION(Class, FuncName, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
	{ Engine::CompileTimeHash(#FuncName), #FuncName, #FuncName, {ReturnTypeName, ReturnTypeEnum}, ParamsSpan, \
	  &Engine::AutoThunk<&Class::FuncName> },

#define REFLECT_FUNCTION_OVERLOAD(Class, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
	{ Engine::CompileTimeHash(SignatureText), DisplayName, SignatureText, {ReturnTypeName, ReturnTypeEnum}, ParamsSpan, \
	  &Engine::AutoThunk<FunctionPtr> },

#define IMPLEMENT_CLASS(ClassName, ParentName) \
	void ClassName::CreateCDO(void* cdoBuf) \
	{ \
		[](auto* tag, void* b) { \
			using T = std::remove_pointer_t<decltype(tag)>; \
			if constexpr (!std::is_abstract_v<T>) { new(b) T(); } \
		}(static_cast<ClassName*>(nullptr), cdoBuf); \
	} \
	void ClassName::DestroyCDO(void* cdoBuf) \
	{ \
		[](auto* tag, void* b) { \
			using T = std::remove_pointer_t<decltype(tag)>; \
			if constexpr (!std::is_abstract_v<T>) { static_cast<T*>(b)->~T(); } \
		}(static_cast<ClassName*>(nullptr), cdoBuf); \
	} \
	void ClassName::CopyCDO(void* dst, const void* src) \
	{ \
		[](auto* tag, void* d, const void* s) { \
			using T = std::remove_pointer_t<decltype(tag)>; \
			if constexpr (!std::is_abstract_v<T>) { \
				*static_cast<T*>(d) = *static_cast<const T*>(s); \
			} \
		}(static_cast<ClassName*>(nullptr), dst, src); \
	} \
	void* ClassName::HeapNew() \
	{ \
		if constexpr (!std::is_abstract_v<ClassName>) return new ClassName(); \
		return nullptr; \
	} \
	static constexpr Engine::TypeInfo ClassName##_TypeInfo = { \
		Engine::CompileTimeHash(#ClassName), #ClassName, #ParentName, sizeof(ClassName), \
		ClassName::GetProperties(), \
		ClassName##_Functions, \
		std::is_abstract_v<ClassName> ? nullptr : &ClassName::CreateCDO, \
		std::is_abstract_v<ClassName> ? nullptr : &ClassName::DestroyCDO, \
		std::is_abstract_v<ClassName> ? nullptr : &ClassName::CopyCDO, \
		std::is_abstract_v<ClassName> ? nullptr : &ClassName::HeapNew, \
		[](void* inst) { static_cast<ClassName*>(inst)->PostLoad(); } \
	}; \
	const Engine::TypeInfo& ClassName::GetStaticType() { return ClassName##_TypeInfo; } \
	namespace { \
		struct AutoRegister_##ClassName { \
			AutoRegister_##ClassName() { \
				Engine::ReflectionRegistry::Get().RegisterType(ClassName##_TypeInfo.ID, ClassName##_TypeInfo); \
			} \
		}; \
		static AutoRegister_##ClassName g_AutoRegister_##ClassName; \
	}

#define BEGIN_ENUM(EnumName) \
	static constexpr Engine::EnumEntry EnumName##_Entries[] = {

#define REFLECT_ENUM_ENTRY(EnumName, EntryName) \
	{ #EntryName, static_cast<uint64_t>(EnumName::EntryName) },

#define END_ENUM_REFLECT(EnumName) \
	}; \
	static constexpr Engine::EnumInfo EnumName##_EnumInfo = { \
		#EnumName, EnumName##_Entries \
	}; \
	namespace { \
		struct AutoRegister_##EnumName { \
			AutoRegister_##EnumName() { \
				Engine::ReflectionRegistry::Get().RegisterEnum(Engine::CompileTimeHash(#EnumName), EnumName##_EnumInfo); \
			} \
		}; \
		static AutoRegister_##EnumName g_AutoRegister_##EnumName; \
	}

#define NAME(text) {Engine::CompileTimeHash("Name"), text},
#define TOOLTIP(text) {Engine::CompileTimeHash("Tooltip"), text},
#define CATEGORY(text) {Engine::CompileTimeHash("Category"), text},
#define RANGE(...) {Engine::CompileTimeHash("Range"), Engine::MetaRange{ __VA_ARGS__ }},
#define COLOR(r, g, b, a)	{Engine::CompileTimeHash("Color"), Engine::MetaColor{ vec4(r, g, b, a) }},
#define EDITABLE			{Engine::CompileTimeHash("Editable"), true},
#define READONLY			{Engine::CompileTimeHash("ReadOnly"), true},
#define FILEPATH(filter)	{Engine::CompileTimeHash("FilePath"), filter},
#define DIRECTORY			{Engine::CompileTimeHash("Directory"), true},
#define EDITCONDITION(conditionVar, ...) {Engine::CompileTimeHash("EditCondition"), Engine::MetaEditCondition{ conditionVar, __VA_ARGS__ }},
#define DEFAULT(value) {Engine::CompileTimeHash("Default"), std::string_view(#value)},
#define ONCHANGED(...)  {Engine::CompileTimeHash("OnChanged"), Engine::MetaOnChanged{ __VA_ARGS__ }},