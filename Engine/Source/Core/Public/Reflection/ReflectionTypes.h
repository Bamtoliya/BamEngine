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
	function<void(void*, function<void(void*, void*)>)> ForEach;
	function<void(void*)> Add;
	function<void* (void*, size_t)> GetElement;
	function<void(void*, size_t)> Resize;
	function<void(void*)> AddPair;
};

struct PropertyInfo
{
	string Name;
	string TypeName;
	EPropertyType Type;
	size_t Offset;
	ContainerAccessor* Accessor = nullptr;

	PropertyMetadata Metadata;

	PropertyInfo(const string& name, const string& typeName, EPropertyType type, size_t offset, ContainerAccessor* accessor = nullptr)
		: Name(name), TypeName(typeName), Type(type), Offset(offset), Accessor(accessor) {}

	void Apply(const Engine::Name& attr){ Metadata.DisplayName = attr.Text; }
	void Apply(const char* key) { Metadata.DisplayName = key; }
	void Apply(const Engine::Tooltip& attr) { Metadata.Tooltip = attr.Text; }
	void Apply(const EUIWidget& attr) { Metadata.WidgetType = attr; }
	void Apply(const Engine::Range& attr) { Metadata.Min = attr.Min; Metadata.Max = attr.Max; Metadata.WidgetType = EUIWidget::Slider; }
	void Apply(const Engine::ReadOnly& attr) { Metadata.ReadOnly = attr.Value; }
	void Apply(const Color& attr) { Metadata.WidgetType = EUIWidget::Color; }
	void Apply(f32 min, f32 max, f32 speed = 0.1f)
	{
		Metadata.Min = min;
		Metadata.Max = max;
		Metadata.Speed = speed;
		Metadata.WidgetType = EUIWidget::Drag;
	}
	void Apply(const Flags& attr)
	{
		Metadata.BitFlags = attr.Items;
		Metadata.WidgetType = EUIWidget::BitFlag;
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

private:
	string m_Name;
	size_t m_Size;
	vector<PropertyInfo> m_Properties;
};
END