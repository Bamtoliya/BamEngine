#pragma once

#include "Engine_API.h"
#include "Base.h"

BEGIN(Engine)

class ENGINE_API IModule : public Base
{
public:
    virtual ~IModule() = default;

    // 모듈 초기화 (시스템, 컴포넌트, 리플렉션 타입 등록)
    virtual void OnLoad() = 0;

    // 모듈 정리 (등록했던 시스템 및 타입 해제)
    virtual void OnUnload() = 0;

    // 핫리로드 언로드 직전 호출 (상태 임시 캐싱 등 필요한 전처리)
    virtual void OnPreHotReload() {}

    // 핫리로드 로드 직후 호출 (상태 복구 및 재연결)
    virtual void OnPostHotReload() {}
};

END

// C-Linkage 내보내기/가져오기 함수 시그니처
extern "C" {
    typedef Engine::IModule* (*CreateModuleFunc)();
    typedef void (*DestroyModuleFunc)(Engine::IModule*);
}

// 모듈 DLL 프로젝트에서 진입점을 쉽게 선언하기 위한 편의 매크로
#define IMPLEMENT_MODULE(ModuleClass) \
    extern "C" { \
        __declspec(dllexport) Engine::IModule* CreateModule() { return new ModuleClass(); } \
        __declspec(dllexport) void DestroyModule(Engine::IModule* module) { delete module; } \
    }