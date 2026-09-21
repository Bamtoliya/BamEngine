#pragma once

#include "Types.h"

namespace Engine
{
    struct RHIFrameMetrics
    {
        uint64 DrawCalls = 0;
        uint64 IndexedDrawCalls = 0;

        uint64 SubmittedVertices = 0;
        uint64 SubmittedIndices = 0;

        // 렌더 함수가 끝나 통계가 확정되었는지
        bool Available = false;

        // 해당 렌더 호출이 성공했는지
        bool RenderSucceeded = false;

        uint64 GetTotalDrawCalls() const
        {
            return DrawCalls + IndexedDrawCalls;
        }
    };
}