#pragma once

#include "Base.h"

class RHI : public Base {
public:
    virtual ~RHI() = default;

    virtual void Init() = 0;
    virtual void Shutdown() = 0;

    virtual void SetClearColor(const glm::vec4& color) = 0;
    virtual void DrawIndexed(uint32 count) = 0;
    virtual void CreateTexture(void* data, uint32 size) = 0;
};