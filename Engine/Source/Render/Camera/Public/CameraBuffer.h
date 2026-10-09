#pragma once

#include "Types.h"

struct CameraBuffer
{
	Engine::mat4 viewMatrix;
	Engine::mat4 invViewMatrix;
	Engine::mat4 projMatrix;
	Engine::mat4 invProjMatrix;
	Engine::mat4 viewProjMatrix;
	Engine::mat4 invViewProjMatrix;
	Engine::vec3 cameraPosition;
	Engine::f32 time;
};