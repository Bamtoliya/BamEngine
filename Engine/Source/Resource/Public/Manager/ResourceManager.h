#pragma once

#include "Base.h"
#include "Resources.h"

BEGIN(Engine)

#pragma region Generic Interface
struct IResourceContainer
{
	virtual ~IResourceContainer() = default;
	virtual void Clear() = 0;
};

template<typename T>
struct ResourceContainer : public IResourceContainer
{
	unordered_map<wstring, T*> m_Map;
	virtual ~ResourceContainer()
	{
		Clear();
	}
	virtual void Clear() override
	{
		for (auto& pair : m_Map)
		{
			delete pair.second;
		}
		m_Map.clear();
	}

	void Add(const wstring& key, T* resource)
	{
		m_Map.emplace(key, resource);
	}

	T* Get(const wstring& key)
	{
		auto iter = m_Map.find(key);
		if (iter != m_Map.end())
		{
			return iter->second;
		}
		return nullptr;
	}
};
#pragma endregion

class ENGINE_API ResourceManager : public Base
{
	DECLARE_SINGLETON(ResourceManager)
#pragma region Constructor&Destructor
private:
	ResourceManager() {}
	virtual ~ResourceManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion
#pragma region Resource Management
public:
	EResult LoadMesh(const wstring& key, void* arg);
	EResult LoadShader(const wstring& key, void* arg);
	EResult LoadTexture(const wstring& key, const wstring& texturePath);
	EResult LoadMaterial(const wstring& key, void* arg);
public:
	class Mesh* GetMesh(const wstring& key);
	class Shader* GetShader(const wstring& key);
	class Texture* GetTexture(const wstring& key);
	class Material* GetMaterial(const wstring& key);
#pragma region Generic Case
public:
	template<typename T>
	EResult Load(const wstring& key, void* arg)
	{
		ResourceContainer<T>* container = GetContainer<T>();
		if (container->Get(key)) return EResult::AlreadyInitialized;

		T* resource = T::Create(arg);

		if (!resource) return EResult::Fail;

		container->Add(key, resource);

		return EResult::Success;
	}

	template<typename T>
	T* Get(const wstring& key)
	{
		ResourceContainer<T>* container = GetContainer<T>();
		return container->Get(key);
	}
#pragma endregion
public:
	EResult ImportFolder(const wstring& folderPath);
	EResult LoadFile(const wstring& filePath);

	template<typename T>
	void RegisterExtension(const wstring& extension)
	{
		m_LoaderRegistry[extension] = [this](wstring key, wstring path) -> EResult
		{
			return this->Load<T>(key, (void*)&path);
		};
	}

	void RegisterExplicitLoader();
#pragma endregion

#pragma region Helper Functions
private:
	template<typename T>
	ResourceContainer<T>* GetContainer()
	{
		type_index typeIdx = type_index(typeid(T));
		auto iter = m_GenericContainers.find(typeIdx);
		if (iter != m_GenericContainers.end())
		{
			return static_cast<ResourceContainer<T>*>(iter->second);
		}
		else
		{
			ResourceContainer<T>* newContainer = new ResourceContainer<T>();
			m_GenericContainers[typeIdx] = newContainer;
			return newContainer;
		}
	}
#pragma endregion

#pragma region Variable
private:
	unordered_map<wstring, class Mesh*> m_Meshes;
	unordered_map<wstring, class Shader*> m_Shaders;
	unordered_map<wstring, class Texture*> m_Textures;
	unordered_map<wstring, class Material*> m_Materials;
private:
	unordered_map<type_index, IResourceContainer*> m_GenericContainers;
private:
	unordered_map<wstring, function<EResult(wstring, wstring)>> m_LoaderRegistry;
#pragma endregion
};
END