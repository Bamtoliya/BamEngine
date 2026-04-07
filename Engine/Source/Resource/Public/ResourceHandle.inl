#pragma once
#include "ResourceManager.h"

template<typename T>
inline void ResourceHandle<T>::InternalAddRef()
{
	if (m_Handle.IsValid()) ResourceManager::Get().AddRef(m_Handle);
}

template<typename T>
inline void ResourceHandle<T>::InternalRelease()
{
	if (m_Handle.IsValid()) ResourceManager::Get().Release(m_Handle);
}

template<typename T>
inline bool ResourceHandle<T>::IsValid() const
{
	if (!m_Handle.IsValid()) return false;
	return ResourceManager::Get().IsValid(m_Handle);
}

template<typename T>
inline T* ResourceHandle<T>::Get() const
{
	if (!m_Handle.IsValid()) return nullptr;
	return static_cast<T*>(ResourceManager::Get().GetResource(m_Handle));
}