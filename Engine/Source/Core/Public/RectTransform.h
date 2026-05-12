#pragma once

#include "Component.h"



BEGIN(Engine)

#pragma region Enum
ENUM()
enum class EAlignment
{
	TopLeft,	TopCenter,		TopRight,
	MiddleLeft,	MiddleCenter,	MiddleRight,
	BottomLeft, BottomCenter,	BottomRight
};
ENUM()
enum class ERectSpace { Local, Screen };
ENUM()
enum class ERectPivot
{ 
	Custorm = 0,
	TopLeft,	TopCenter,		TopRight,
	MiddleLeft,	MiddleCenter,	MiddleRight,
	BottomLeft, BottomCenter,	BottomRight
};
ENUM()
enum class ERectAnchor
{
	Custorm = 0,
	TopLeft,	TopCenter,		TopRight,
	MiddleLeft,	MiddleCenter,	MiddleRight,
	BottomLeft, BottomCenter,	BottomRight, 

	StretchTop,      // 위쪽에 붙어서 가로로 늘어남
	StretchMiddle,   // 중앙에서 가로로 늘어남
	StretchBottom,   // 아래쪽에 붙어서 가로로 늘어남
	StretchLeft,     // 왼쪽에 붙어서 세로로 늘어남
	StretchCenter,   // 중앙에서 세로로 늘어남
	StretchRight,    // 오른쪽에 붙어서 세로로 늘어남
	StretchFull      // 화면 가득 채움
};
ENUM()
enum class ERectTransformFlags
{
	None = 0,
	LockPositionX = 1 << 0,
	LockPositionY = 1 << 1,
	LockPosition = LockPositionX | LockPositionY,

	LockWidth = 1 << 2,
	LockHeight = 1 << 3,
	LockSize = LockWidth | LockHeight,

	LockRotation = 1 << 4,
	LockScale = 1 << 5,

	IgnoreLayout = 1 << 6,
	InheritAlpha = 1 << 7,

	RaycastTarget = 1 << 8,

	Culled = 1 << 9,

	LockRatio = 1 << 10,

	AllLocked	= LockPosition | LockSize | LockRotation | LockScale,
	Default		= InheritAlpha | RaycastTarget
};
ENABLE_BITMASK_OPERATORS(ERectTransformFlags)
#pragma endregion


struct tagRectTransformCreateDesc : public tagComponentDesc
{
	vec2 Size = { 100.0f, 100.0f };
	vec2 AnchorMin = { 0.5f, 0.5f };
	vec2 AnchorMax = { 0.5f, 0.5f };
	vec2 AnchoredPosition = { 0.0f, 0.0f };
	vec2 Pivot = { 0.5f, 0.5f };
	vec2 Scale = { 1.0f, 1.0f };
	f32	 Rotation = 0.0f;
	ERectTransformFlags Flags = ERectTransformFlags::Default;
};

CLASS()
class ENGINE_API RectTransform final : public Component
{
	REFLECT_CLASS()
	using DESC = tagRectTransformCreateDesc;
#pragma region Constructor&Destructor
private:
	RectTransform() : Component{} {}
	virtual ~RectTransform() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Component* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void Update(f32 dt) override;
private:
	void UpdateMatrix();
#pragma endregion

#pragma region Getter
public:
	const vec2& GetSize() const { return m_Size; }
	const vec2& GetAnchorMin() const { return m_AnchorMin; }
	const vec2& GetAnchorMax() const { return m_AnchorMax; }
	const vec2& GetAnchoredPosition() const { return m_AnchoredPosition; }
	const vec2& GetPivot() const { return m_Pivot; }
	const vec2& GetScale() const { return m_Scale; }
	const f32& GetRotation() const { return m_Rotation; }
	const ERectTransformFlags& GetFlags() const { return m_Flags; }

	const mat4& GetMatrix() const { return m_Matrix; }
	const vec2& GetAbsolutePosition() const { return m_AbsolutePosition; }
	const vec2& GetAbsoluteSize() const { return m_AbsoluteSize; }
#pragma endregion
#pragma region Setter
public:
	FUNCTION()
	void SetSize(const vec2& size);
	FUNCTION()
	void SetAnchorMin(const vec2& anchorMin);
	FUNCTION()
	void SetAnchorMax(const vec2& anchorMax);
	FUNCTION()
	void SetAnchoredPosition(const vec2& anchoredPosition);
	FUNCTION()
	void SetPivot(const vec2& pivot);
	FUNCTION()
	void SetScale(const vec2& scale);
	FUNCTION()
	void SetRotation(const f32& rotation);
	FUNCTION()
	void SetFlags(const ERectTransformFlags& flags);

	FUNCTION()
	void SetAnchorPreset(ERectAnchor preset);
	FUNCTION()
	void SetPivotPreset(ERectPivot preset);
#pragma endregion



#pragma region Member Variables
private:
	PROPERTY(NAME("PROP_POSITION"), EDITABLE, RANGE(), ONCHANGED("SetAnchoredPosition", "m_AnchoredPosition"))
	vec2 m_AnchoredPosition = { 0.0f, 0.0f };

	PROPERTY(EDITABLE, RANGE(), ONCHANGED("SetRotation", "m_Rotation"))
	f32 m_Rotation = { 0.0f };

	PROPERTY(EDITABLE, RANGE(), ONCHANGED("SetSize", "m_Size"))
	vec2 m_Size = { 100.0f, 100.0f };

	PROPERTY(EDITABLE, RANGE(), ONCHANGED("SetScale", "m_Scale"))
	vec2 m_Scale = { 1.0f, 1.0f };

	PROPERTY(EDITABLE, RANGE(), CATEGORY("Anchor"), ONCHANGED("SetAnchorMin", "m_AnchorMin"))
	vec2 m_AnchorMin = { 0.5f, 0.5f };
	PROPERTY(EDITABLE, RANGE(), CATEGORY("Anchor"), ONCHANGED("SetAnchorMax", "m_AnchorMax"))
	vec2 m_AnchorMax = { 0.5f, 0.5f };
	
	PROPERTY(EDITABLE, RANGE(0.f, 1.f, 0.01f), ONCHANGED("SetPivot", "m_Pivot"))
	vec2 m_Pivot = { 0.5f, 0.5f };

	PROPERTY(EDITABLE, ONCHANGED("SetFlags", "m_Flags"))
	ERectTransformFlags m_Flags = { ERectTransformFlags::Default };

	PROPERTY(NAME("PROP_MATRIX"), READONLY, NOSERIALIZE, CATEGORY("Details"))
	mat4 m_Matrix = glm::identity<mat4>();
	PROPERTY(NAME("PROP_ABSOLUTEPOSITION"), READONLY, NOSERIALIZE, CATEGORY("Details"))
	vec2 m_AbsolutePosition = { 0.0f, 0.0f };
	PROPERTY(NAME("PROP_ABSOLUTESIZE"), READONLY, NOSERIALIZE, CATEGORY("Details"))
	vec2 m_AbsoluteSize = { 0.0f, 0.0f };

	PROPERTY(NAME("PROP_RATIO"), READONLY, NOSERIALIZE, CATEGORY("Details"))
	f32 m_Ratio = { 1.f };
#pragma endregion

};
END