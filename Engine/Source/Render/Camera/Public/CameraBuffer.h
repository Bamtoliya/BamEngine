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

    static CameraBuffer FromMatrices(const Engine::mat4& view, const Engine::mat4& proj,
        const Engine::vec3& position, Engine::f32 frameTime)
    {
        CameraBuffer buffer{};
        buffer.viewMatrix = view;
        buffer.invViewMatrix = glm::inverse(view);
        buffer.projMatrix = proj;
        buffer.invProjMatrix = glm::inverse(proj);
        buffer.viewProjMatrix = proj * view;
        buffer.invViewProjMatrix = glm::inverse(buffer.viewProjMatrix);
        buffer.cameraPosition = position;
        buffer.time = frameTime;
        return buffer;
    }
};