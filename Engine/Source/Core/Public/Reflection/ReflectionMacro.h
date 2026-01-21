#pragma once

#include "ReflectionRegistry.h"
#include <cstddef>

#define PROPERTY(...)
#define CLASS()
#define STRUCT()


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

#define REFLECT_PROPERTY(VarName, TypeEnum, TypeName) \
	typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, TypeEnum, offsetof(ThisClass, VarName)));

#define REFLECT_BITFLAG(VarName, TypeEnum, TypeName) \
	typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, TypeEnum, offsetof(ThisClass, VarName)));

#define REFLECT_VECTOR(VarName, FullType, InnerType, TypeName) \
	{ \
		static Engine::ContainerAccessor acc = Engine::LinearContainerAccessor<FullType, InnerType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::List, offsetof(ThisClass, VarName), &acc)); \
	}

#define REFLECT_LIST(VarName, InnerType, TypeName) \
	{ \
		static Engine::ContainerAccessor acc = Engine::ListAccessor<InnerType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::List, offsetof(ThisClass, VarName), &acc)); \
	}

#define REFLECT_SET(VarName, FullType, InnerType, TypeName) \
	{ \
		static Engine::ContainerAccessor acc = Engine::SetAccessor<FullType, InnerType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::Set, offsetof(ThisClass, VarName), &acc)); \
	}

#define REFLECT_MAP(VarName, FullType, KeyType, ValueType, TypeName) \
	{ \
		static Engine::ContainerAccessor acc = Engine::MapAccessor<FullType, KeyType, ValueType>::Get(); \
		typeInfo.AddProperty(Engine::PropertyInfo(#VarName, TypeName, Engine::EPropertyType::Map, offsetof(ThisClass, VarName), &acc)); \
	}

#define END_REFLECT() }