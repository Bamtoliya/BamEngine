#pragma once
#include "Base.h"
#include <SDL3/SDL.h>

BEGIN(Editor)

class ImGuiManager final : public Base
{
	DECLARE_SINGLETON(ImGuiManager)

public:
	typedef struct tagImGuiCreateInfo
	{
		SDL_Window* Window = { nullptr };
		SDL_Renderer* Renderer = { nullptr };
	}IMGUISDLDESC;

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
#pragma region Variables
private:
	SDL_Window* m_Window = { nullptr };
	SDL_Renderer* m_Renderer = { nullptr };
#pragma endregion



};

END