#pragma once

#include "PropertyMetadata.h"
#include "ReflectionContainers.h"

using namespace std;

BEGIN(Engine)

enum class EPropertyType : uint8
{
	None = 0,
	Int8, Int16, Int32, Int64,
	UInt8, UInt16, UInt32, UInt64,
	F32, F64,
	Float, Double, Bool,
	String,
	Wstring, 
	Vector2, Vector3, Vector4, Color,
	Quaternion,
	Matrix3, Matrix4,

	Object,
	Enum,
	BitFlag,

	Struct,
	Array,
	List,
	Map,
	Set
};

struct VariableInfo
{ 
	string_view Name;
	EPropertyType Type;
};

struct ContainerInfo
{
	VariableInfo Inner;
	VariableInfo Key;

	const ContainerAccessor* Accessor = nullptr;
};

struct PropertyInfo
{
	uint64 ID;
	string_view Name;
	VariableInfo Type;
	size_t Offset;
	size_t Size;

	const ContainerInfo* ContainerData = nullptr;
	span<const MetadataEntry> Metadata;
};

struct EnumEntry
{
	string_view Name;
	uint64 Value;
};

struct EnumInfo
{
	string_view Name;
	span<const EnumEntry> Entries;
	string GetBitFlagsString(uint64 value) const
	{
		if (value == 0)
		{
			for (const auto& [name, entryVal] : Entries)
			{
				if (entryVal == 0) return string(name); 
			}
			return "None";
		}

		string result;
		for (const auto& [name, entryVal] : Entries)
		{
			if (entryVal == 0) continue;

			if ((value & entryVal) == entryVal)
			{
				if (!result.empty()) result += " | ";
				result += name;
			}
		}

		return result.empty() ? std::to_string(value) : result;
	}
};

typedef void* (*InvokeFunctionPtr)(void* instance, void** args);

struct FunctionInfo
{
	uint64 ID;
	string_view Name;
	VariableInfo ReturnType;
	span<const VariableInfo> Parameters;

	InvokeFunctionPtr InvokeFunc = nullptr;

	void* Invoke(void* instance, void** args)
	{
		if (InvokeFunc) return InvokeFunc(instance, args);
		return nullptr;
	}
};

struct TypeInfo
{
	uint64 ID;
	string_view Name;
	string_view ParentName;
	size_t Size;
	span<const PropertyInfo> Properties;
	span<const FunctionInfo> Functions;

	using CDOCreateFunc = void(*)(void* buffer);           
	using CDODestroyFunc = void(*)(void* buffer);          
	using CDOCopyFunc = void(*)(void* dst, const void* src);

	CDOCreateFunc  Create = nullptr;
	CDODestroyFunc Destroy = nullptr;
	CDOCopyFunc    Copy = nullptr;
};
END