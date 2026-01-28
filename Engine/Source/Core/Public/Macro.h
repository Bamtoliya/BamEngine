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