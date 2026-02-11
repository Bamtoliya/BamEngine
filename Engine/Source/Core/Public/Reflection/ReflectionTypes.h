#pragma once

#include "Types.h"
#include <functional>
#include "PropertyMetadata.h"
#include "PropertyAttributes.h"

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
	List,
	Map,
	Set
};

struct ContainerAccessor
{
	function<size_t(const void*)> GetSize;
	function<void(void*)> Clear;
	function<void(void*, function<void(void*, void*)>)> ForEach;
	function<void(void*, const void*)> Add;
	function<void(void*, const void*)> Remove;
	function<void* (void*, size_t)> GetElement;
	function<vector<void*>(void*)> GetElements;
	function<void(void*, size_t)> Resize;
	function<void(void*)> AddPair;
	function<void* (void*, const void*)> GetValue;

	struct Iterator
	{
		void* Ptr;
	};

	function<void* (const void*)> Begin;
	function<void* (const void*)> End;
	function<void(void*)> Next;
	function<void* (void*)> GetVal;
	function<void(void*)> DestroyIter;
};

struct PropertyInfo
{
	string Name;
	string TypeName;
	EPropertyType Type;
	size_t Offset;
	size_t Size;

	string InnerTypeName;
	string KeyTypeName;
	ContainerAccessor* Accessor = nullptr;
	//const TypeInfo* ClassTypeInfo = nullptr;	

	PropertyMetadata Metadata;

	PropertyInfo(const string& name, const string& typeName, EPropertyType type, size_t offset, ContainerAccessor* accessor = nullptr, const string& innerTypeName = "", const string& keyTypeName = "", void* arg = nullptr)
		: Name(name), TypeName(typeName), Type(type), Offset(offset), Accessor(accessor), InnerTypeName(innerTypeName), KeyTypeName(keyTypeName) {}

	PropertyInfo(const string& name, const string& typeName, EPropertyType type, size_t offset, size_t size, ContainerAccessor* accessor = nullptr, const string& innerTypeName = "", const string& keyTypeName = "", void* arg = nullptr)
		: Name(name), TypeName(typeName), Type(type), Offset(offset), Size(size), Accessor(accessor), InnerTypeName(innerTypeName), KeyTypeName(keyTypeName) {}

	void Apply(const Engine::Name& attr){ Metadata.DisplayName = attr.Text; }
	void Apply(const char* key) { Metadata.DisplayName = key; }
	void Apply(const Engine::Tooltip& attr) { Metadata.Tooltip = attr.Text; }
	void Apply(const Engine::Category& attr) { Metadata.Category = attr.Text; }
	void Apply(const Engine::Range& attr) { Metadata.Min = attr.Min; Metadata.Max = attr.Max; Metadata.bHasRange = true; Metadata.Speed = attr.Speed; }
	void Apply(const Engine::ReadOnly& attr) { Metadata.bIsReadOnly = attr.bEnable; }
	void Apply(const Engine::FilePath& attr) { Metadata.bIsFilePath = true; Metadata.FileFilter = attr.Filter; }
	void Apply(const Engine::Directory& attr) { Metadata.bIsDirectory = true; Metadata.DialogPath = attr.Path; }
	void Apply(const Color& attr) {}
	void Apply(f32 min, f32 max, f32 speed = 1.f)
	{
		Metadata.Min = min;
		Metadata.Max = max;
		Metadata.Speed = speed;
		Metadata.bHasRange = true;
	}
	void Apply(const Flags& attr)
	{
		Metadata.BitFlags = attr.Items;
	}
	void Apply(const Engine::Default& attr)
	{
		Metadata.DefaultValue = attr.Value;
		Metadata.bHasDefault = true;
	}
	void Apply(const Engine::EditCondition& attr)
	{
		Metadata.EditCondition = attr.ConditionVariableName;
	}
	void Apply(const Engine::EditConditionBit& attr)
	{
		Metadata.EditCondtionBit = attr.ConditionVariableName;
		Metadata.EditConditionMask = attr.Mask;
		Metadata.bEditConditionBit = true;
		Metadata.bEditConditionExact = attr.bExactMatch;
	}
	template <typename T, typename... Args, typename = std::enable_if_t<(sizeof...(Args) > 0)>>
	void Apply(T&& first, Args&&... args)
	{
		Apply(std::forward<T>(first));
		Apply(std::forward<Args>(args)...);
	}
	void Apply() {}
};

struct EnumInfo
{
	string Name;
	unordered_map<string, uint64> Entries;
	vector<pair<string, uint64>> OrderedEntries;
	string GetBitFlagsString(uint64 value) const
	{
		if (value == 0)
		{
			for (const auto& [name, entryVal] : Entries)
			{
				if (entryVal == 0) return name; 
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

class TypeInfo
{
public:
	TypeInfo(const string& name, size_t size)
		: m_Name(name), m_Size(size) {}
	const string& GetName() const { return m_Name; }
	size_t GetSize() const { return m_Size; }
	const vector<PropertyInfo>& GetProperties() const { return m_Properties; }

	template<typename... Args>
	void AddProperty(const PropertyInfo& proeperty, Args&&... args)
	{
		m_Properties.push_back(proeperty);
		PropertyInfo& prop = m_Properties.back();
		(prop.Apply(forward<Args>(args)), ...);
	}

	void AddProperty(const PropertyInfo& property)
	{
		m_Properties.push_back(property);
	}

	void SetParentName(const string& parentName) { m_ParentName = parentName; }
	const string& GetParentName() const { return m_ParentName; }
private:
	string m_Name;
	string m_ParentName;
	size_t m_Size;
	vector<PropertyInfo> m_Properties;
};
END