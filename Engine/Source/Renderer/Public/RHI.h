#pragma once
#include "Base.h"

BEGIN(Engine)

typedef struct tagRHICreateInfo
{
	void* WindowHandle = { nullptr };
	uint32 Width = { 0 };
    uint32 Height = { 0 };
    bool IsVSync = { true };
} RHICREATEINFO;

class ENGINE_API RHI : public Base
{
protected:
    RHI() {}
    virtual ~RHI() = default;
    virtual EResult Initialize(void* arg) PURE;
public:
    virtual void Free() override {};
public:
    virtual void Shutdown() PURE;
    virtual void Resize() PURE;
public:
    virtual EResult BeginFrame() PURE;
    virtual EResult EndFrame() PURE;
public:
    virtual void SetVertexBuffer(void* arg) PURE;
	virtual void SetIndexBuffer(void* arg) PURE;


#pragma region Bind
    virtual void BindPipeline(void* arg) PURE;
    virtual void BindVertexBuffer(void* arg) PURE;
    virtual void BindIndexBuffer(void* arg) PURE;

    virtual void BindConstantBuffer(void* arg, uint32 slot) PURE;
    virtual void BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) PURE;
#pragma endregion


#pragma region Draw

    virtual void Draw(uint32 count) PURE;
    virtual void DrawIndexed(uint32 count) PURE;
    virtual void DrawIndexedInstanced() PURE;
#pragma endregion

#pragma region Setter
    virtual void SetClearColor(void* arg) PURE;
    virtual void SetViewport(void* arg) PURE;
#pragma endregion

#pragma region Getter
    virtual void* GetNativeRHI() const PURE;
#pragma endregion
    virtual void CreateTexture(void* data, uint32 size)  PURE;
};

END