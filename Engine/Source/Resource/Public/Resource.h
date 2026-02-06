#pragma once

#include "Base.h"
#include "ReflectionMacro.h"

struct tagResourceCreateDesc
{
	wstring		Tag = L"";
};

BEGIN(Engine)

CLASS()
class ENGINE_API Resource : public Base
{
	REFLECT_CLASS(Resource)
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



#pragma region Member Variables
protected:
	PROPERTY()
	wstring m_Tag = {};

	PROPERTY(READONLY)
	uint32 m_Version = { 0 };
#pragma endregion
};
END