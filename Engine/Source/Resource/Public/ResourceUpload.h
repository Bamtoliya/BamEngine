#pragma once

#include "Engine_API.h"

namespace Engine
{
    class RHI;

    // 이 범위가 살아 있는 스레드에서 만든 리소스는 GPU 객체를 만들지 않는다.
    // 나중에 메인 스레드에서 Resource::UploadToGPU를 호출해야 한다.
    class ENGINE_API DeferredResourceUpload final
    {
    public:
        DeferredResourceUpload();
        ~DeferredResourceUpload();
        DeferredResourceUpload(const DeferredResourceUpload&) = delete;
        DeferredResourceUpload& operator=(const DeferredResourceUpload&) = delete;

    private:
        bool m_Previous = false;
    };

    // 리소스 GPU 객체를 만들 RHI. 렌더러가 없거나 업로드를 미루는 스레드에서는 nullptr.
    ENGINE_API RHI* GetResourceUploadRHI();
}
