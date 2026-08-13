#include "SerializationHelper.h"

#include "Interface/Component.h"
#include "GameObject.h"
#include "Archive.h"
#include <vector>

namespace Engine
{
    void SerializationHelper::SerializeEnTT(Archive& ar, entt::meta_any& instance)
    {
        if (!instance) return;

        using namespace entt::literals;

        entt::meta_type type = instance.type();

        for (auto [id, data] : type.data()) 
        {
            // EnTT 3.12+ 에서는 .name() 으로 프로퍼티 이름을 바로 가져올 수 있습니다
            const char* propName = data.name();
            if (!propName) continue; 

            entt::meta_any propValue = data.get(instance);
            if (!propValue) continue;
            
            entt::meta_type valueType = propValue.type();

            if (valueType == entt::resolve<int32>()) {
                int32 val = propValue.cast<int32>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<uint32>()) {
                uint32 val = propValue.cast<uint32>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<float>()) {
                float val = propValue.cast<float>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<bool>()) {
                bool val = propValue.cast<bool>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<glm::vec2>()) {
                glm::vec2 val = propValue.cast<glm::vec2>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<glm::vec3>()) {
                glm::vec3 val = propValue.cast<glm::vec3>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<glm::vec4>()) {
                glm::vec4 val = propValue.cast<glm::vec4>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<glm::quat>()) {
                glm::quat val = propValue.cast<glm::quat>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<std::string>()) {
                std::string val = propValue.cast<std::string>();
                ar.Process(propName, val);
                if (ar.IsReading()) data.set(instance, val);
            }
            else if (valueType == entt::resolve<std::vector<Component*>>()) {
                std::vector<Component*>& vec = *propValue.cast<std::vector<Component*>*>();
                if (ar.IsWriting()) {
                    ar.BeginArray(propName);
                    for (Component* comp : vec) {
                        ar.BeginArrayElement();
                        if (comp) {
                            entt::meta_type compType = entt::resolve(entt::type_id(*comp));
                            if (compType) {
                                std::string typeName = std::string(compType.info().name());
                                ar.Process("__Type__", typeName);
                                entt::meta_any compAny{ std::ref(*comp) };
                                SerializeEnTT(ar, compAny);
                            }
                        }
                        ar.EndArrayElement();
                    }
                    ar.EndArray();
                } else {
                    size_t count = ar.BeginArray(propName);
                    for (size_t i = 0; i < count; ++i) {
                        ar.BeginArrayElement();
                        std::string typeName;
                        ar.Process("__Type__", typeName);
                        if (!typeName.empty()) {
                            entt::meta_type compType = entt::resolve(entt::hashed_string(typeName.c_str()));
                            if (compType) {
                                entt::meta_any newComp = compType.construct();
                                if (newComp) {
                                    Component* compPtr = newComp.cast<Component*>();
                                    SerializeEnTT(ar, newComp);
                                    vec.push_back(compPtr);
                                }
                            }
                        }
                        ar.EndArrayElement();
                    }
                    ar.EndArray();
                }
            }
            else if (valueType == entt::resolve<std::vector<GameObject*>>()) {
                std::vector<GameObject*>& vec = *propValue.cast<std::vector<GameObject*>*>();
                if (ar.IsWriting()) {
                    ar.BeginArray(propName);
                    for (GameObject* obj : vec) {
                        ar.BeginArrayElement();
                        if (obj) {
                            entt::meta_type objType = entt::resolve(entt::type_id(*obj));
                            if (objType) {
                                std::string typeName = std::string(objType.info().name());
                                ar.Process("__Type__", typeName);
                                entt::meta_any objAny{ std::ref(*obj) };
                                SerializeEnTT(ar, objAny);
                            }
                        }
                        ar.EndArrayElement();
                    }
                    ar.EndArray();
                } else {
                    size_t count = ar.BeginArray(propName);
                    for (size_t i = 0; i < count; ++i) {
                        ar.BeginArrayElement();
                        std::string typeName;
                        ar.Process("__Type__", typeName);
                        if (!typeName.empty()) {
                            entt::meta_type objType = entt::resolve(entt::hashed_string(typeName.c_str()));
                            if (objType) {
                                entt::meta_any newObj = objType.construct();
                                if (newObj) {
                                    GameObject* objPtr = newObj.cast<GameObject*>();
                                    SerializeEnTT(ar, newObj);
                                    vec.push_back(objPtr);
                                }
                            }
                        }
                        ar.EndArrayElement();
                    }
                    ar.EndArray();
                }
            }
            else {
                if (ar.PushScope(propName)) {
                    SerializeEnTT(ar, propValue);
                    if (ar.IsReading()) {
                        data.set(instance, propValue);
                    }
                    ar.PopScope();
                }
            }
        }
    }
}