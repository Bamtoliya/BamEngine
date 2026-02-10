#pragma once

#include "ReflectionRegistry.h"
#include <cstddef>
#include <type_traits>

#define PROPERTY(...)
#define CLASS()
#define STRUCT()
#define ENUM(...)


#define REFLECT_STRUCT(StructName) \
	using ThisClass = StructName; \
	static Engine::TypeInfo& GetStaticType() \
	{ \
		static Engine::TypeInfo& type = Engine::ReflectionRegistry::Get().RegisterType(#StructName, sizeof(ThisClass)); \
		static bool bInitialized = false; \
		if (!bInitialized) \
		{ \
			RegisterProperties(type); \
			bInitialized = true; \
		} \
		return type; \
	} \
	virtual Engine::TypeInfo& GetType() const { return ThisClass::GetStaticType(); } \
	static void RegisterProperties(Engine::TypeInfo& typeInfo);
 
#define REFLECT_CLASS(ClassName) \
public: \
	using ThisClass = ClassName; \
	static Engine::TypeInfo& GetStaticType() \
	{ \
		static Engine::TypeInfo& type = Engine::ReflectionRegistry::Get().RegisterType(#ClassName, sizeof(ThisClass)); \
		static bool bInitialized = false; \
		if (!bInitialized) \
		{ \
			RegisterProperties(type); \
			bInitialized = true; \
		} \
		return type; \
	} \
	virtual Engine::TypeInfo& GetType() const { return ThisClass::GetStaticType(); } \
private: \
	static void RegisterProperties(Engine::TypeInfo& typeInfo);

#define BEGIN_REFLECT(ClassName) \
	void ClassName::RegisterProperties(Engine::TypeInfo& typeInfo) {

#define REFLECT_PROPERTY(VarName, TypeEnum, TypeName, ...) \
	typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, TypeEnum, offsetof(ThisClass, VarName), sizeof(ThisClass::VarName)), ##__VA_ARGS__);

#define REFLECT_BITFLAG(VarName, TypeEnum, TypeName, ...) \
	typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, TypeEnum, offsetof(ThisClass, VarName), sizeof(ThisClass::VarName)), ##__VA_ARGS__);

#define REFLECT_VECTOR(VarName, FullType, InnerType, TypeName, ...) \
	{ \
		static Engine::ContainerAccessor acc = Engine::LinearContainerAccessor<FullType, InnerType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::List, offsetof(ThisClass, VarName), &acc), ##__VA_ARGS__); \
	}

#define REFLECT_LIST(VarName, InnerType, TypeName, ...) \
	{ \
		static Engine::ContainerAccessor acc = Engine::ListAccessor<InnerType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::List, offsetof(ThisClass, VarName), &acc), ##__VA_ARGS__); \
	}

#define REFLECT_SET(VarName, FullType, InnerType, TypeName, ...) \
	{ \
		static Engine::ContainerAccessor acc = Engine::SetAccessor<FullType, InnerType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::Set, offsetof(ThisClass, VarName), &acc, #InnerType), ##__VA_ARGS__); \
	}

#define REFLECT_MAP(VarName, ValueTypeName, KeyTypeName, ...) \
	{ \
		static Engine::ContainerAccessor acc = Engine::MapAccessor<decltype(ThisClass::VarName)>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, ValueTypeName, Engine::EPropertyType::Map, offsetof(ThisClass, VarName), &acc, ValueTypeName, KeyTypeName), ##__VA_ARGS__); \
	}

#define REFLECT_PARENT(ParentName) \
	typeInfo.SetParentName(#ParentName);

#define END_REFLECT() }

#define REFLECT_STATIC_TYPE(TypeName) \
	TypeName::GetStaticType();  

#define BEGIN_ENUM_REFLECT(EnumName) \
    struct Reflector_##EnumName { \
        Reflector_##EnumName() { \
            std::unordered_map<std::string, uint64> entries;

#define REFLECT_ENUM_ENTRY(EnumName, EntryName) \
            entries[#EntryName] = (uint64)EnumName::EntryName;

#define END_ENUM_REFLECT(EnumName) \
            Engine::ReflectionRegistry::Get().RegisterEnum(#EnumName, entries); \
        } \
    }; \
    static Reflector_##EnumName global_##EnumName##_reflector;



#define NAME(text) Engine::Name(text)
#define TOOLTIP(text) Engine::Tooltip(text)
#define CATEGORY(text) Engine::Category(text)
#define RANGE(...) Engine::Range({ __VA_ARGS__ })
#define READONLY Engine::ReadOnly(true)
#define FILEPATH(filter) Engine::FilePath(filter)
#define DIRECTORY Engine::Directory()
#define COLOR(r, g, b, a) Engine::Color(vec4(r, g, b, a))
#define FLAGS(...) Engine::Flags({ __VA_ARGS__ })
#define DEFAULT(value) Engine::Default(value)
