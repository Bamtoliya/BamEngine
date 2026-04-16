#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(REFLECTION_CORE_EXPORTS)
#define REFLECTION_CORE_API __declspec(dllexport)
#else
#define REFLECTION_CORE_API __declspec(dllimport)
#endif
#else
#define REFLECTION_CORE_API
#endif