#pragma once
#include "Application.h"

using namespace Editor;

int main(int argc, char* argv[])
{
    ApplicationCreateInfo createInfo = {};
    Application* app = Application::Create(&createInfo);
    if (!app)
        return 1;

    app->Run(argc, argv);
    Application::Destroy();
    return 0;
}