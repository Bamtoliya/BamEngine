#pragma once

#include "Base.h"
#include "RenderTarget.h"

enum class EViewportMode : uint8
{
	Wireframe,
	Solid,
	Textured,
};

enum class EViewportCameraType : uint8
{
	Perspective,
	Orthographic,
};

enum class EViewportType : uint8
{
	Scene,
	Game,
	Editor,
	Max
};

BEGIN(Editor)

class ViewportPanel
{
public:
	ViewportPanel() {};
	virtual ~ViewportPanel() = default;
private:
	void Initialize(void* arg = nullptr);

public:
	void Draw();
#pragma region Members
private:
	bool m_IsFocused = false;
	bool m_IsHovered = false;
#pragma endregion
};
END