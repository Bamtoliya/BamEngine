#include "ResourceUpload.h"
#include "Renderer.h"

namespace
{
    thread_local bool t_DeferResourceUpload = false;
}

Engine::DeferredResourceUpload::DeferredResourceUpload()
    : m_Previous(t_DeferResourceUpload)
{
    t_DeferResourceUpload = true;
}

Engine::DeferredResourceUpload::~DeferredResourceUpload()
{
    t_DeferResourceUpload = m_Previous;
}

Engine::RHI* Engine::GetResourceUploadRHI()
{
    return !t_DeferResourceUpload && Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
}
