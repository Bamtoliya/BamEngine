#pragma once

#include "Base.h"
#include "RHI.h"

BEGIN(Engine)

enum class ERHIType
{
	Unknown,
	SDLRenderer,
	D3D11,
	D3D12,
	Vulkan,
	OpenGL,
	Metal
};

typedef struct tagRendererCreateInfo
{
	ERHIType RHIType = ERHIType::Unknown;
	void* WindowHandle = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;
	bool IsVSync = true;
} RENDERERDESC;

class Renderer final : public Base
{
	DECLARE_SINGLETON(Renderer)

	friend class Runtime;

#pragma region Constructor&Destructor
private:
	Renderer() {}
	virtual ~Renderer() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

private:
	EResult BeginFrame();
	EResult EndFrame();
public:
	class RHI* GetRHI() const { return m_RHI; }

private:
	class RHI* m_RHI;

};
END