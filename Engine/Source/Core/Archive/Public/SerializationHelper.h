#pragma once

#include "Archive.h"
#include "ReflectionRegistry.h"
#include "PropertyAccessor.h"

BEGIN(Engine)

class SerializationHelper
{
public:
	static void SerializeReflectionProperties(Archive& ar, const TypeInfo* typeInfo, void* instance)
	{
		if (!typeInfo || !instance) return;

		for (const auto& prop : typeInfo->Properties)
		{
			string_view propName = prop.Name;
			void* valuePtr = PropertyAccessor::GetValuePtr(instance, prop);
			SerializeValue(ar, propName, prop.Type, valuePtr, prop.Size, prop.ContainerData);
		}
	}

private:
	static string NormalizeTypeName(string_view typeName)
	{
		string name(typeName);

		if (name.starts_with("class "))
			name.erase(0, 6);
		else if (name.starts_with("struct "))
			name.erase(0, 7);

		while (!name.empty() && (name.back() == '*' || name.back() == ' '))
			name.pop_back();

		return name;
	}

	static size_t GetValueSize(const VariableInfo& varInfo, size_t explicitSize = 0)
	{
		if (explicitSize != 0)
			return explicitSize;

		switch (varInfo.Type)
		{
		case EPropertyType::Int8:       return sizeof(int8);
		case EPropertyType::Int16:      return sizeof(int16);
		case EPropertyType::Int32:      return sizeof(int32);
		case EPropertyType::Int64:      return sizeof(int64);
		case EPropertyType::UInt8:      return sizeof(uint8);
		case EPropertyType::UInt16:     return sizeof(uint16);
		case EPropertyType::UInt32:     return sizeof(uint32);
		case EPropertyType::UInt64:     return sizeof(uint64);
		case EPropertyType::F32:
		case EPropertyType::Float:      return sizeof(f32);
		case EPropertyType::F64:
		case EPropertyType::Double:     return sizeof(f64);
		case EPropertyType::Bool:       return sizeof(bool);
		case EPropertyType::String:     return sizeof(string);
		case EPropertyType::Wstring:    return sizeof(wstring);
		case EPropertyType::Vector2:    return sizeof(glm::vec2);
		case EPropertyType::Vector3:    return sizeof(glm::vec3);
		case EPropertyType::Vector4:
		case EPropertyType::Color:      return sizeof(glm::vec4);
		case EPropertyType::Quaternion: return sizeof(glm::quat);
		case EPropertyType::Matrix3:    return sizeof(glm::mat3);
		case EPropertyType::Matrix4:    return sizeof(glm::mat4);
		case EPropertyType::Enum:
		case EPropertyType::BitFlag:    return sizeof(int64); // 임시 기본값
		default:                        return 0;
		}
	}
	// -------------------------------------------------------
	// Map Key: void* → string (쓰기 방향)
	// -------------------------------------------------------
	static string KeyToString(EPropertyType type, const void* keyPtr)
	{
		switch (type)
		{
			// --- 문자열 ---
		case EPropertyType::String:  return *static_cast<const string*>(keyPtr);
		case EPropertyType::Wstring: return Engine::WStrToStr(*static_cast<const wstring*>(keyPtr));

			// --- 정수형 ---
		case EPropertyType::Int8:    return std::to_string(*static_cast<const int8*>(keyPtr));
		case EPropertyType::Int16:   return std::to_string(*static_cast<const int16*>(keyPtr));
		case EPropertyType::Int32:   return std::to_string(*static_cast<const int32*>(keyPtr));
		case EPropertyType::Int64:   return std::to_string(*static_cast<const int64*>(keyPtr));

		case EPropertyType::UInt8:   return std::to_string(*static_cast<const uint8*>(keyPtr));
		case EPropertyType::UInt16:  return std::to_string(*static_cast<const uint16*>(keyPtr));
		case EPropertyType::UInt32:  return std::to_string(*static_cast<const uint32*>(keyPtr));
		case EPropertyType::UInt64:  return std::to_string(*static_cast<const uint64*>(keyPtr));

			// --- 실수형 ---
		case EPropertyType::F32:     return std::format("{}", *static_cast<const f32*>(keyPtr));
		case EPropertyType::F64:     return std::format("{}", *static_cast<const f64*>(keyPtr));

			// --- 수학 & GLM 타입 ---
		case EPropertyType::Vector2: { const auto& v = *static_cast<const glm::vec2*>(keyPtr); return std::format("{},{}", v.x, v.y); }
		case EPropertyType::Vector3: { const auto& v = *static_cast<const glm::vec3*>(keyPtr); return std::format("{},{},{}", v.x, v.y, v.z); }
		case EPropertyType::Vector4:
		case EPropertyType::Color: { const auto& v = *static_cast<const glm::vec4*>(keyPtr); return std::format("{},{},{},{}", v.x, v.y, v.z, v.w); }
		case EPropertyType::Quaternion: { const auto& v = *static_cast<const glm::quat*>(keyPtr); return std::format("{},{},{},{}", v.x, v.y, v.z, v.w); }
		case EPropertyType::Matrix3: {
			const auto& m = *static_cast<const glm::mat3*>(keyPtr);
			return std::format("{},{},{},{},{},{},{},{},{}",
				m[0][0], m[0][1], m[0][2],
				m[1][0], m[1][1], m[1][2],
				m[2][0], m[2][1], m[2][2]);
		}
		case EPropertyType::Matrix4: {
			const auto& m = *static_cast<const glm::mat4*>(keyPtr);
			return std::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
				m[0][0], m[0][1], m[0][2], m[0][3],
				m[1][0], m[1][1], m[1][2], m[1][3],
				m[2][0], m[2][1], m[2][2], m[2][3],
				m[3][0], m[3][1], m[3][2], m[3][3]);
		}

								   // --- 열거형 및 비트플래그 ---
		case EPropertyType::Enum:
		case EPropertyType::BitFlag: {
			// 메타데이터에 Key의 바이트 크기(Size) 정보가 없으므로 가장 넓은 범위인 int32로 취급합니다.
			return std::to_string(*static_cast<const int32*>(keyPtr));
		}

								   // --- 복합 타입 (구조체, 배열, 리스트, 맵, 세트 등) ---
		case EPropertyType::Struct:
		case EPropertyType::Object:
		case EPropertyType::Array:
		case EPropertyType::List:
		case EPropertyType::Map:
		case EPropertyType::Set: {
			// JSON 표준은 객체나 배열을 Key로 허용하지 않습니다. (오직 문자열만 가능)
			// 구조체나 컨테이너를 맵의 키로 쓰려면 "Array of Key-Value Pairs" 방식으로 별도 직렬화해야 합니다.
			return "Unsupported_Complex_Key";
		}

		default: return {};
		}
	}

	// -------------------------------------------------------
	// Map Key: string → void* (읽기 방향, 즉시 실행 콜백)
	// -------------------------------------------------------
	template<typename Func>
	static void WithParsedKey(EPropertyType type, const string& keyStr, Func&& fn)
	{
		switch (type)
		{
			// --- 문자열 ---
		case EPropertyType::String: { string   v = keyStr;                    fn(&v); break; }
		case EPropertyType::Wstring: { wstring  v = Engine::StrToWStr(keyStr); fn(&v); break; }

								   // --- 정수형 ---
		case EPropertyType::Int8: { int8     v = static_cast<int8>(std::stoi(keyStr));  fn(&v); break; }
		case EPropertyType::Int16: { int16    v = static_cast<int16>(std::stoi(keyStr)); fn(&v); break; }
		case EPropertyType::Int32: { int32    v = std::stoi(keyStr);                     fn(&v); break; }
		case EPropertyType::Int64: { int64    v = std::stoll(keyStr);                    fn(&v); break; }

		case EPropertyType::UInt8: { uint8    v = static_cast<uint8>(std::stoul(keyStr));  fn(&v); break; }
		case EPropertyType::UInt16: { uint16   v = static_cast<uint16>(std::stoul(keyStr)); fn(&v); break; }
		case EPropertyType::UInt32: { uint32   v = std::stoul(keyStr);                      fn(&v); break; }
		case EPropertyType::UInt64: { uint64   v = std::stoull(keyStr);                     fn(&v); break; }

								  // --- 실수형 ---
		case EPropertyType::F32: { f32      v = std::stof(keyStr);         fn(&v); break; }
		case EPropertyType::F64: { f64      v = std::stod(keyStr);         fn(&v); break; }

							   // --- 수학 & GLM 타입 ---
		case EPropertyType::Vector2: { glm::vec2 v{}; sscanf_s(keyStr.c_str(), "%f,%f", &v.x, &v.y); fn(&v); break; }
		case EPropertyType::Vector3: { glm::vec3 v{}; sscanf_s(keyStr.c_str(), "%f,%f,%f", &v.x, &v.y, &v.z); fn(&v); break; }
		case EPropertyType::Vector4:
		case EPropertyType::Color: { glm::vec4 v{}; sscanf_s(keyStr.c_str(), "%f,%f,%f,%f", &v.x, &v.y, &v.z, &v.w); fn(&v); break; }
		case EPropertyType::Quaternion: { glm::quat v{}; sscanf_s(keyStr.c_str(), "%f,%f,%f,%f", &v.x, &v.y, &v.z, &v.w); fn(&v); break; }
		case EPropertyType::Matrix3: {
			glm::mat3 m{};
			sscanf_s(keyStr.c_str(), "%f,%f,%f,%f,%f,%f,%f,%f,%f",
				&m[0][0], &m[0][1], &m[0][2],
				&m[1][0], &m[1][1], &m[1][2],
				&m[2][0], &m[2][1], &m[2][2]);
			fn(&m); break;
		}
		case EPropertyType::Matrix4: {
			glm::mat4 m{};
			sscanf_s(keyStr.c_str(), "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
				&m[0][0], &m[0][1], &m[0][2], &m[0][3],
				&m[1][0], &m[1][1], &m[1][2], &m[1][3],
				&m[2][0], &m[2][1], &m[2][2], &m[2][3],
				&m[3][0], &m[3][1], &m[3][2], &m[3][3]);
			fn(&m); break;
		}

		// --- 열거형 및 비트플래그 ---
		case EPropertyType::Enum:
		case EPropertyType::BitFlag: {
			int32 v = std::stoi(keyStr);
			fn(&v); break;
		}

		// --- 복합 타입 (구조체, 배열, 리스트, 맵, 세트 등) ---
		case EPropertyType::Struct:
		case EPropertyType::Object:
		case EPropertyType::Array:
		case EPropertyType::List:
		case EPropertyType::Map:
		case EPropertyType::Set: {
			// 복합 타입은 단일 문자열로 파싱하기 매우 어렵습니다.
			// 안전을 위해 nullptr을 콜백으로 넘겨 무시합니다.
			fn(nullptr); break;
		}

		default: fn(nullptr); break;
		}
	}

	static void SerializeValue(Archive& ar, string_view propName, const VariableInfo& varInfo, void* valuePtr, size_t size, const ContainerInfo* containerData)
	{
		if (!valuePtr) return;
		switch (varInfo.Type)
		{
		case Engine::EPropertyType::Int8: return ar.Process(propName, *static_cast<int8*>(valuePtr));
		case Engine::EPropertyType::Int16: return ar.Process(propName, *static_cast<int16*>(valuePtr));
		case Engine::EPropertyType::Int32: return ar.Process(propName, *static_cast<int32*>(valuePtr));
		case Engine::EPropertyType::Int64: return ar.Process(propName, *static_cast<int64*>(valuePtr));
		case Engine::EPropertyType::UInt8: return ar.Process(propName, *static_cast<uint8*>(valuePtr));
		case Engine::EPropertyType::UInt16: return ar.Process(propName, *static_cast<uint16*>(valuePtr));
		case Engine::EPropertyType::UInt32: return ar.Process(propName, *static_cast<uint32*>(valuePtr));
		case Engine::EPropertyType::UInt64: return ar.Process(propName, *static_cast<uint64*>(valuePtr));
		case Engine::EPropertyType::F32: 
		case Engine::EPropertyType::Float: return ar.Process(propName, *static_cast<f32*>(valuePtr));
		case Engine::EPropertyType::F64: 
		case Engine::EPropertyType::Double:return ar.Process(propName, *static_cast<f64*>(valuePtr));
		case Engine::EPropertyType::Bool: return ar.Process(propName, *static_cast<bool*>(valuePtr));
		case Engine::EPropertyType::String: return ar.Process(propName, *static_cast<string*>(valuePtr));
		case Engine::EPropertyType::Wstring: return ar.Process(propName, *static_cast<wstring*>(valuePtr));
		case Engine::EPropertyType::Vector2: return ar.Process(propName, *static_cast<glm::vec2*>(valuePtr));
		case Engine::EPropertyType::Vector3: return ar.Process(propName, *static_cast<glm::vec3*>(valuePtr));
		case Engine::EPropertyType::Vector4:
		case Engine::EPropertyType::Color: return ar.Process(propName, *static_cast<glm::vec4*>(valuePtr));
		case Engine::EPropertyType::Quaternion: return ar.Process(propName, *static_cast<glm::quat*>(valuePtr));
		case Engine::EPropertyType::Matrix3: return ar.Process(propName, *static_cast<glm::mat3*>(valuePtr));
		case Engine::EPropertyType::Matrix4: return ar.Process(propName, *static_cast<glm::mat4*>(valuePtr));
		case Engine::EPropertyType::Enum:
		case Engine::EPropertyType::BitFlag: return ar.ProcessEnum(propName, valuePtr, size);
		case Engine::EPropertyType::Object:
		case Engine::EPropertyType::Struct:
		{
			if (ar.PushScope(propName))
			{
				const TypeInfo* InnerTypeInfo = ReflectionRegistry::Get().GetType(NormalizeTypeName(varInfo.Name));
				if (InnerTypeInfo)
				{
					SerializeReflectionProperties(ar, InnerTypeInfo, valuePtr);
				}
				ar.PopScope();
			}
			break;
		}
		case Engine::EPropertyType::Array:
		case Engine::EPropertyType::List:
		case Engine::EPropertyType::Set:
		{
			if (containerData && containerData->Accessor)
			{
				const size_t innerSize = GetValueSize(containerData->Inner);

				if (ar.IsWriting())
				{
					ar.BeginArray(propName);
					vector<void*> elements = containerData->Accessor->GetElements(valuePtr);
					for (void* element : elements)
					{
						ar.BeginArrayElement();
						SerializeValue(ar, "", containerData->Inner, element, innerSize, containerData->InnerContainerData);
						ar.EndArrayElement();
					}
					ar.EndArray();
				}
				else
				{
					size_t count = ar.BeginArray(propName);
					if (containerData->Accessor->Resize)
					{
						containerData->Accessor->Resize(valuePtr, count);
						vector<void*> elements = containerData->Accessor->GetElements(valuePtr);
						for(size_t i = 0; i < count; ++i)
						{
							ar.BeginArrayElement();
							SerializeValue(ar, "", containerData->Inner, elements[i], innerSize, containerData->InnerContainerData);
							ar.EndArrayElement();
						}
					}
					else
					{

					}
					ar.EndArray();
				}
			}
			break;
		}
		case Engine::EPropertyType::Map:
		{
			if (!containerData || !containerData->Accessor)
				break;

			const size_t innerSize = GetValueSize(containerData->Inner);
			if (ar.IsWriting())
			{
				ar.BeginMap(propName);
				struct MapContext
				{
					Archive* ar;
					const ContainerInfo* containerData;
				};
				MapContext context{ &ar, containerData };

				auto callback = [](void* element, void* key, void* userData) 
				{
					MapContext* ctx = static_cast<MapContext*>(userData);
					string keyStr = KeyToString(ctx->containerData->Key.Type, key);
					if (keyStr.empty()) return;

			
					ctx->ar->BeginMapElement(keyStr);
					SerializeValue(*ctx->ar, "", ctx->containerData->Inner, element, GetValueSize(ctx->containerData->Inner), ctx->containerData->InnerContainerData);
					ctx->ar->EndMapElement();
				};
				containerData->Accessor->ForEach(valuePtr, callback, &context);
				ar.EndMap();
			}
			else
			{
				size_t size = ar.BeginMap(propName);
				containerData->Accessor->Clear(valuePtr);

				for (size_t i = 0; i < size; ++i)
				{
					string keyStr;
					ar.BeginMapElement(keyStr);
					WithParsedKey(containerData->Key.Type, keyStr, [&](void* keyPtr)
					{
						if (!keyPtr || !containerData->Accessor->AddAndGetElement) return;
						void* newVal = containerData->Accessor->AddAndGetElement(valuePtr, keyPtr);
						if (newVal)
						{
							SerializeValue(ar, "", containerData->Inner, newVal, 0, containerData->InnerContainerData);
						}
					});
					ar.EndMapElement();
				}

				ar.EndMap();
			}
			break;
		}
		default:
			break;
		}
	}
};
END