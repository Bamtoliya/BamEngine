#pragma once

#define BAMEGINE_VERSION "0.1.0"

#pragma region DLL
// 윈도우일 때만 동작
#ifdef _WIN32
#ifdef ENGINE_EXPORTS
    // DLL을 빌드하는 중이라면: 밖으로 내보낸다 (Export)
#define ENGINE_API __declspec(dllexport)
#else
    // DLL을 사용하는 중이라면(Game): 안으로 가져온다 (Import)
#define ENGINE_API __declspec(dllimport)
#endif
#else
    // 리눅스/맥은 기본적으로 다 공개됨
#define ENGINE_API
#endif
#pragma endregion