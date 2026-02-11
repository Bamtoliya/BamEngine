#pragma once

#include "Base.h"
#include "ReflectionMacro.h"

BEGIN(Engine)
#pragma region Struct
STRUCT()
struct tagComponentDesc
{
	REFLECT_STRUCT(tagComponentDesc)

	PROPERTY()
	bool Active = { true };

	PROPERTY()
	wstring Tag = { L"" };

	PROPERTY()
	class GameObject* Owner = { nullptr };
};
#pragma endregion

class GameObject;

class ENGINE_API Component abstract : public Base
{
	using DESC = tagComponentDesc;
	REFLECT_CLASS(Component)
#pragma region Constructor&Destructor
protected:
	Component() {}
	virtual ~Component() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) PURE;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void FixedUpdate(f32 dt) {}
	virtual void Update(f32 dt) {}
	virtual void LateUpdate(f32 dt) {}
	virtual EResult Render(f32 dt) { return EResult::Success; }
#pragma endregion

#pragma region Getter
public:
	GameObject* GetOwner() const { return m_Owner; }
	wstring GetTag() const { return m_Tag; }
	bool IsActive() const { return m_Active; }
	bool IsDirty() const { return m_Dirty; }
#pragma endregion

#pragma region Setter
public:
	void SetOwner(GameObject* owner) { m_Owner = owner; }
	void SetTag(const wstring& tag) { m_Tag = tag; }
	void SetActive(bool active) { m_Active = active; }
	void SetDirty(bool dirty = true) { m_Dirty = dirty; }
#pragma endregion

#pragma region Variable
protected:
	bool m_Active = { true };
	bool m_Dirty = { false };
	wstring m_Tag = { L"" };
	GameObject* m_Owner = { nullptr };
#pragma endregion
};

END