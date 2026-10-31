#pragma once

#include "Types.h"
#include <string>
#include <vector>

namespace Engine
{
    // 렌더 패스별 CPU/GPU 실행 시간
    struct RHIPassMetric
    {
        std::wstring PassName;
        f32 CPUTimeMs = 0.0f;
        f32 GPUTimeMs = 0.0f;
    };

    // 하드웨어 및 렌더 드라이버 기능 지원 여부
    struct RHICapabilities
    {
        bool SupportsCompute = false;
        bool SupportsIndirectDraw = false;
        bool SupportsMeshShaders = false;
        bool SupportsRaytracing = false;
        bool SupportsDescriptorIndexing = false; // Bindless
        uint32 MaxComputeWorkGroupInvocations = 0;
    };

    // GPU 비디오 메모리(VRAM) 및 디스크립터 통계
    struct RHIMemoryStats
    {
        uint64 DedicatedVRAMBudgetMB = 0;
        uint64 DedicatedVRAMUsageMB = 0;
        uint64 SharedSystemMemoryUsageMB = 0;
        uint32 AllocatedDescriptors = 0;
    };

    struct RHIFrameMetrics
    {
        uint64 DrawCalls = 0;
        uint64 IndexedDrawCalls = 0;
        uint64 SubmittedVertices = 0;
        uint64 SubmittedIndices = 0;

        // 프레임 전체 CPU 제출 시간 및 GPU 실행 시간
        f32 TotalCPUTimeMs = 0.0f;
        f32 TotalGPUTimeMs = 0.0f;

        // 패스별 세부 메트릭 목록
        std::vector<RHIPassMetric> PassMetrics;

        // GPU 메모리 상태
        RHIMemoryStats MemoryStats;

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