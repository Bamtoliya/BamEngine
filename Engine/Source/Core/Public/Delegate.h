#pragma once

#include "Base.h"
BEGIN(Engine)

using DelegateHandle = uint64;

template<typename RetVal, typename... ParamTypes>
class Delegate
{
public:
	using FunctionType = function<RetVal(ParamTypes...)>;
public:
	void Bind(FunctionType func)
	{
		m_Function = func;
	}

	void Unbind()
	{
		m_Function = nullptr;
	}

	RetVal Execute(ParamTypes... args) const
	{
		if (!m_Function)
		{
			// Handle the case where no function is bound; could throw an exception or return a default value
			throw runtime_error("No function bound to delegate.");
		}
		return m_Function(args...);
	}

	void ExecuteIfBound(ParamTypes... args) const
	{
		if (m_Function)
		{
			m_Function(args...);
		}
	}
private:
	FunctionType m_Function = nullptr;
};

template<typename... ParamTypes>
class MulticastDelegate
{
public:
	using FunctionType = function<void(ParamTypes...)>;
private:
	struct Listener
	{
		DelegateHandle Handle;
		FunctionType Function;
	};
public:
	DelegateHandle Add(FunctionType func)
	{
		DelegateHandle newHandle = ++m_CurrentHandle;
		m_Listeners.push_back({ newHandle, func });
		return newHandle;
	}

	DelegateHandle AddLambda(FunctionType func)
	{
		return Add(func);
	}

	void Remove(DelegateHandle handle)
	{
		std::erase_if()(m_Listeners, [handle](const Listener& listener) {
			return listener.Handle == handle;
		});
	}

	void Clear()
	{
		m_Listeners.clear();
	}

	void Broadcast(ParamTypes... args) const
	{
		for (const auto& listener : m_Listeners)
		{
			listener.Function(args...);
		}
	}

	bool IsBound() const
	{
		return !m_Listeners.empty();
	}

private:
	vector<Listener> m_Listeners;
	DelegateHandle m_CurrentHandle = 0;
};

END