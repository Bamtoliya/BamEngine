#pragma once
#include <cassert>

#define PURE =0

#define BEGIN(NAMESPACE) namespace NAMESPACE {
#define END }

#define NO_COPY(ClassName)								\
ClassName(const ClassName&) = delete;					\
ClassName& operator=(const ClassName&) = delete;

#pragma region Singleton
#define DECLARE_SINGLETON(ClassName)					\
		NO_COPY(ClassName)								\
public:													\
	static ClassName* Create(void* arg = nullptr);		\
	static ClassName& Get();							\
	static int32_t Destroy();							\
private:												\
	static ClassName* m_instance;		

#define IMPLEMENT_SINGLETON(ClassName)					\
ClassName* ClassName::m_instance = nullptr;				\
														\
ClassName* ClassName::Create(void* arg)					\
{														\
	if (!m_instance)									\
	{													\
		m_instance = new ClassName();					\
		if(IsFailure(m_instance->Initialize(arg)))		\
		{												\
			delete m_instance;							\
			m_instance = nullptr;						\
		}												\
	}													\
	return m_instance;									\
}														\
														\
ClassName& ClassName::Get()								\
{														\
	assert(m_instance != nullptr && "Error: Singleton Instance not created yet!");\
	return *m_instance;									\
}														\
														\
int32_t ClassName::Destroy()							\
{														\
	if (m_instance)										\
	{													\
		int32_t count = m_instance->Release();			\
		if (count == 0)									\
		{												\
			m_instance = nullptr;						\
		}												\
		return count;									\
	}													\
	return 0;											\
}	

#define IMPLEMENT_LAZY_SINGLETON(ClassName)				\
ClassName* ClassName::m_instance = nullptr;				\
														\
ClassName* ClassName::Create(void* arg)					\
{														\
	if (!m_instance)									\
	{													\
		m_instance = new ClassName();					\
		if(IsFailure(m_instance->Initialize(arg)))		\
		{												\
			delete m_instance;							\
			m_instance = nullptr;						\
		}												\
	}													\
	return m_instance;									\
}														\
														\
ClassName& ClassName::Get()								\
{														\
	if (!m_instance)									\
	{													\
		Create(nullptr);								\
	}													\
	return *m_instance;									\
}														\
														\
int32_t ClassName::Destroy()							\
{														\
	if (m_instance)										\
	{													\
		int32_t count = m_instance->Release();			\
		if (count == 0)									\
		{												\
			m_instance = nullptr;						\
		}												\
		return count;									\
	}													\
	return 0;											\
}
#pragma endregion

#pragma region Release Macro
#define RELEASE_MAP(MapType)						\
	for (auto& pair : MapType)						\
		Safe_Release(pair.second);					\
	MapType.clear();

#define RELEASE_VECTOR(VectorType)					\
	for (auto& element : VectorType)				\
		Safe_Release(element);						\
	VectorType.clear();
#pragma endregion

#define CAST_DESC DESC* desc = reinterpret_cast<DESC*>(arg);

#pragma region Component Register Macro
#define REGISTER_COMPONENT(ClassName)				\
	static Engine::ComponentRegisterHelper<ClassName> _reg_##ClassName(L#ClassName);
#pragma endregion

/******************************************************************************/
#define MAX_LAYER_COUNT 32

#define DEFAULT_PPU 1.f
#define GRAVITY -9.81f

/*****************************************************************************/
// 문자열 변환을 위한 보조 매크로
#define TODO_STR2(x) #x
#define TODO_STR1(x) TODO_STR2(x)

// 1. 컴파일러 출력창(Terminal)에 메시지를 띄우는 매크로
// 출력창의 메시지를 클릭하면 해당 라인으로 바로 이동할 수 있습니다.

#if defined(_MSC_VER)
	// MSVC: "파일경로(라인번호) : 키워드: 메시지" 형식으로 출력하여 더블클릭 이동 지원
#define TODO(msg)  __pragma(message(__FILE__ "(" TODO_STR1(__LINE__) ") : TODO: " msg))
#define FIXME(msg) __pragma(message(__FILE__ "(" TODO_STR1(__LINE__) ") : FIXME: " msg))
#elif defined(__GNUC__) || defined(__clang__)
	// GCC/Clang: _Pragma를 이용한 메시지 출력
#define DO_PRAGMA(x) _Pragma(#x)
#define TODO(msg)  DO_PRAGMA(message ("TODO: " msg))
#define FIXME(msg) DO_PRAGMA(message ("FIXME: " msg))
#else
#define TODO(msg)
#define FIXME(msg)
#endif

// 2. 컴파일을 중단시키는 강력한 리마인더 (C++17 static_assert 활용)
// 구현이 누락되면 빌드 자체가 안 되게 막아줍니다.
#define TODO_ERROR(msg)  static_assert(false, "CRITICAL TODO: " msg)
#define FIXME_ERROR(msg) static_assert(false, "CRITICAL FIXME: " msg)
