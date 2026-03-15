#pragma once

#include "Base.h"
#include "ReflectionMacro.h"
#include "SerializableInterface.h"

struct tagResourceCreateDesc
{
	wstring		Tag = L"";
	wstring		Path = L"";
};

BEGIN(Engine)

CLASS()
class ENGINE_API Resource : public Base, public SerializableInterface
{
	REFLECT_BASE()
	using DESC = tagResourceCreateDesc;
#pragma region Constructor&Destructor
protected:
	Resource() {}
	virtual ~Resource() = default;
	virtual EResult Initialize(void* arg = nullptr) {
		if (arg)
		{
			CAST_DESC
			m_Tag = desc->Tag;
		}
		return EResult::Success; 
	}
public:
	virtual void Free() PURE;
#pragma	endregion


#pragma region Bind
	virtual EResult Bind(uint32 slot) PURE;
#pragma endregion


#pragma region Load
public:
	virtual EResult LoadFromFile(const wstring& path) { return EResult::NotImplemented; }
#pragma endregion

#pragma region Tag Management
public:
	wstring GetTag() { return m_Tag; }
	void SetTag(const wstring& tag) { m_Tag = tag; }
#pragma endregion

#pragma region Version Management
public:
	uint32 GetVersion() const { return m_Version; }
	void IncreaseVersion() { m_Version++; }
#pragma endregion

#pragma region Path Management
public:
	const wstring& GetPath() const { return m_Path; }
	void SetPath(const wstring& path) { m_Path = path; }
#pragma endregion

#pragma region Save & Load
public:
	virtual void Serialize(Archive& ar) override { SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this); }
	virtual void Deserialize(Archive& ar) override { Serialize(ar); }
#pragma endregion



#pragma region Member Variables
protected:
	PROPERTY(READONLY)
	wstring m_Tag = {};

	PROPERTY(READONLY)
	uint32 m_Version = { 0 };

	PROPERTY(READONLY)
	wstring m_Path = {};
#pragma endregion
};
END