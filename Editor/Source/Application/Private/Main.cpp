#pragma once
#include "Application.h"

using namespace Editor;

int main(int argc, char* argv[])
{
	Application* app = Application::Create();
	if (app)
	{
		app->Run(argc, argv);
		Application::Destroy();
	}
	return 0;
}