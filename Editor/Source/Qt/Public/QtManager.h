#pragma once

#include <memory>
#include "Base.h"

class QMainWindow;

BEGIN(Editor)

struct QtManagerDesc
{
	int Width = 1280;
	int Height = 720;
	const char* Title = "BamEngine Editor";
};

class QtManager final : public Engine::Base
{
	DECLARE_SINGLETON(QtManager)

#pragma region Constructor&Destructor
private:
	QtManager();
	~QtManager() override;
	Engine::EResult Initialize(void* arg = nullptr);
public:
	void Free() override;
#pragma endregion

public:
	void Show();

private:
	std::unique_ptr<QMainWindow> m_MainWindow;
};

END