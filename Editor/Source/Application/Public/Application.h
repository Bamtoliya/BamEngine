#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)

struct ApplicationCreateInfo
{
	// 필요한 초기화 정보를 여기에 추가
	uint32 width = 1280;
	uint32 height = 720;
	const char* AppName = "BamEngine Editor";
};

class Application final : public Base
{
	DECLARE_SINGLETON(Application)
#pragma region Constructor&Destructor
private:
	Application() {}
	virtual ~Application() = default;
	EResult Initialize(void* arg);
public:
	void Free() override;
#pragma endregion

public:
	void Run(int argc, char* argv[]);
	void Shutdown();
public:
	void SetResolution(uint32 width, uint32 height, bool fullscreen);
	void GetWindowSize(int32* w, int32* h) const;
private:
	void UpdateTitle(f32 dt);
	void Test(f32 dt);
private:
	class Runtime* m_Runtime = { nullptr };
	
	struct SDL_Window* m_Window = { nullptr };
	class RHI* m_RHI = { nullptr };
private:
	f32 m_FPSTimer = { 0.f };
};

END