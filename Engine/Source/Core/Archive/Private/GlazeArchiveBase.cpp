#pragma once

#include "GlazeArchiveBase.h"

#pragma region Scope
bool GlazeArchiveBase::PushScope(string_view key)
{
	auto* currentTop = m_ScopeStack.empty() ? &m_Root : m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t();
		m_ScopeStack.push(&(*currentTop)[keyStr]);
		return true;
	}
	else
	{
		if (currentTop->is_object())
		{
			auto& obj = currentTop->get_object();
			auto it = obj.find(keyStr);
			if (it != obj.end())
			{
				m_ScopeStack.push(&it->second);
				return true;
			}
		}
	}

	return false;
}

void GlazeArchiveBase::PopScope()
{
	if (m_ScopeStack.size() > 1) m_ScopeStack.pop();
}
#pragma endregion //Scope



void GlazeArchiveBase::Process(string_view key, bool& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = v;
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_boolean())
		{
			v = it->second.get_boolean();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, int32& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = static_cast<f64>(v);
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_number())
		{
			v = it->second.get_number();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, uint32& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = static_cast<f64>(v);
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_number())
		{
			v = it->second.get_number();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, uint64& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = static_cast<f64>(v);
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_number())
		{
			v = it->second.get_number();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, f32& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = static_cast<f64>(v);
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_number())
		{
			v = it->second.get_number();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, f64& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = static_cast<f64>(v);
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_number())
		{
			v = it->second.get_number();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, string& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = v;
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_string())
		{
			v = it->second.get_string();
		}
	}
}

void GlazeArchiveBase::Process(string_view key, wstring& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = Engine::WStrToStr(v);
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_string())
		{
			v = Engine::StrToWStr(it->second.get_string());
		}
	}
}

void GlazeArchiveBase::Process(string_view key, glm::vec2& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t::array_t{ v.x, v.y };
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_array())
		{
			auto& arr = it->second.get_array();
			if (arr.size() >= 2)
			{
				v.x = it->second.get_array()[0].get_number();
				v.y = it->second.get_array()[1].get_number();
			}
		}
	}
}

void GlazeArchiveBase::Process(string_view key, glm::vec3& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t::array_t{ v.x, v.y, v.z };
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_array())
		{
			auto& arr = it->second.get_array();
			if (arr.size() >= 3)
			{
				v.x = it->second.get_array()[0].get_number();
				v.y = it->second.get_array()[1].get_number();
				v.z = it->second.get_array()[2].get_number();
			}
		}
	}
}

void GlazeArchiveBase::Process(string_view key, glm::vec4& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t::array_t{ v.x, v.y, v.z, v.w };
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_array())
		{
			auto& arr = it->second.get_array();
			if (arr.size() >= 4)
			{
				v.x = it->second.get_array()[0].get_number();
				v.y = it->second.get_array()[1].get_number();
				v.z = it->second.get_array()[2].get_number();
				v.w = it->second.get_array()[3].get_number();
			}
		}
	}
}

void GlazeArchiveBase::Process(string_view key, glm::quat& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t::array_t{ v.x, v.y, v.z, v.w };
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_array())
		{
			auto& arr = it->second.get_array();
			if (arr.size() >= 4)
			{
				v.x = it->second.get_array()[0].get_number();
				v.y = it->second.get_array()[1].get_number();
				v.z = it->second.get_array()[2].get_number();
				v.w = it->second.get_array()[3].get_number();
			}
		}
	}
}

void GlazeArchiveBase::Process(string_view key, glm::mat4& v)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		glz::json_t::array_t arr;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				arr.push_back(v[i][j]);
			}
		}
		(*currentTop)[keyStr] = arr;
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_array())
		{
			auto& arr = it->second.get_array();
			if (arr.size() >= 16)
			{
				int index = 0;
				for (int i = 0; i < 4; ++i)
				{
					for (int j = 0; j < 4; ++j)
					{
						v[i][j] = static_cast<f32>(arr[index++].get_number());
					}
				}
			}
		}
	}
}

void GlazeArchiveBase::ProcessEnum(string_view key, void* enumPtr, size_t size)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		double val = 0;
		switch (size)
		{
		case 1: val = static_cast<double>(*static_cast<uint8*>(enumPtr)); break;
		case 2: val = static_cast<double>(*static_cast<uint16*>(enumPtr)); break;
		case 4: val = static_cast<double>(*static_cast<uint32*>(enumPtr)); break;
		case 8: val = static_cast<double>(*static_cast<uint64*>(enumPtr)); break;
		}
		(*currentTop)[keyStr] = val;
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_number()) {
			double val = it->second.get_number();

			switch (size)
			{
			case 1: *static_cast<uint8*>(enumPtr) = static_cast<uint8>(val); break;
			case 2: *static_cast<uint16*>(enumPtr) = static_cast<uint16>(val); break;
			case 4: *static_cast<uint32*>(enumPtr) = static_cast<uint32>(val); break;
			case 8: *static_cast<uint64*>(enumPtr) = static_cast<uint64>(val); break;
			}
		}
	}
}

size_t GlazeArchiveBase::BeginArray(string_view key)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t::array_t{};
		m_ScopeStack.push(&(*currentTop)[keyStr]);
		m_ArrayIndexStack.push(0);
		return 0;
	}
	else if (IsReading() && currentTop->is_object())
	{
		auto& obj = currentTop->get_object();
		auto it = obj.find(keyStr);
		if (it != obj.end() && it->second.is_array())
		{
			m_ScopeStack.push(&it->second);
			m_ArrayIndexStack.push(0);
			return it->second.get_array().size();
		}
	}

	return 0;
}

void GlazeArchiveBase::EndArray()
{
	if (m_ScopeStack.size() > 1) {
		m_ScopeStack.pop();
		if (!m_ArrayIndexStack.empty()) m_ArrayIndexStack.pop();
	}
}

void GlazeArchiveBase::BeginArrayElement()
{
	auto* currentArray = m_ScopeStack.top();
	size_t currentIndex = m_ArrayIndexStack.top();

	if (IsWriting())
	{
		currentArray->get_array().push_back(glz::json_t::object_t{});
		m_ScopeStack.push(&currentArray->get_array()[currentIndex]);
	}
	else
	{
		if (currentArray->is_array() && currentIndex < currentArray->get_array().size())
		{
			m_ScopeStack.push(&currentArray->get_array()[currentIndex]);
		}
		else
		{
			m_ScopeStack.push(&m_Root);
		}
	}
}

void GlazeArchiveBase::EndArrayElement()
{
	if (m_ScopeStack.size() > 1)
	{
		m_ScopeStack.pop();
		if (!m_ArrayIndexStack.empty())
		{
			m_ArrayIndexStack.top()++;
		}
	}
}

size_t GlazeArchiveBase::BeginMap(string_view key)
{
	auto* currentTop = m_ScopeStack.top();
	string keyStr(key);

	if (IsWriting())
	{
		(*currentTop)[keyStr] = glz::json_t::object_t{};
		m_ScopeStack.push(&(*currentTop)[keyStr]);
	}
	else
	{
		if (currentTop->is_object())
		{
			auto& obj = currentTop->get_object();
			auto it = obj.find(keyStr);
			if (it != obj.end() && it->second.is_object())
			{
				m_ScopeStack.push(&it->second);
				m_MapIteratorStack.push(it->second.get_object().begin());
				return it->second.get_object().size();
			}
		}
	}
	return 0;
}

void GlazeArchiveBase::EndMap()
{
	if (m_ScopeStack.size() > 1)
	{
		m_ScopeStack.pop();
		if (IsReading() && !m_MapIteratorStack.empty()) m_MapIteratorStack.pop();
	}
}

void GlazeArchiveBase::BeginMapElement(string& outKey)
{
	auto* currentMap = m_ScopeStack.top();

	if (IsWriting())
	{
		(*currentMap)[outKey] = glz::json_t::object_t{};
		m_ScopeStack.push(&(*currentMap)[outKey]);
	}
	else
	{
		if (currentMap->is_object() && !m_MapIteratorStack.empty())
		{
			auto& it = m_MapIteratorStack.top();
			if (it != currentMap->get_object().end())
			{
				outKey = it->first;
				m_ScopeStack.push(&it->second);
			}
			else
			{
				m_ScopeStack.push(&m_Root); // Push root or some default if no more elements
			}
		}
	}
}

void GlazeArchiveBase::EndMapElement()
{
	if (m_ScopeStack.size() > 1)
	{
		m_ScopeStack.pop();
		if (IsReading() && !m_MapIteratorStack.empty())
		{
			auto* currentMap = m_ScopeStack.top();
			auto& it = m_MapIteratorStack.top();
			if (currentMap->is_object() && it != currentMap->get_object().end())
			{
				++it;
			}
		}
	}
}