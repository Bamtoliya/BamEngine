#pragma once

#include "Result.h"

namespace Editor
{
    class ImportJob
    {
    public:
        virtual ~ImportJob() = default;

        // 작업 스레드에서 한 번 호출합니다.
        // 성공은 CPU 준비 완료이며, 전체 Import 완료가 아닙니다.
        virtual Engine::EResult Prepare() = 0;
    };
}