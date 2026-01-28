#pragma once
#include "Base.h"
#include <SDL3/SDL.h>

BEGIN(Editor)

struct tagImGuiManagerDesc
{
	SDL_Window* Window = { nullptr };
	class RHI* RHI = { nullptr };
};

class ImGuiManager final : public Base
{
	DECLARE_SINGLETON(ImGuiManager)
	using DESC = tagImGuiManagerDesc;
#pragma region Constructor&Destructor
private:
	ImGuiManager() {}
	virtual ~ImGuiManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free()override;
#pragma endregion

public:
	void Begin();
	void End();
	void ProcessEvent(const SDL_Event* event);
	void Draw();

private:
	void ToolBar();
	void MainDockspace();
#pragma region Variables
private:
	SDL_Window* m_Window = { nullptr };
	class RHI* m_RHI = { nullptr };
#pragma endregion



};

END