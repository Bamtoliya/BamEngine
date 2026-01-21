import os
import re
import sys

# 1. 경로 설정
ENGINE_SOURCE_DIR = "Engine/Source"
OUTPUT_FILE = os.path.abspath("Engine/Source/Core/Private/Reflection/Reflection.gen.cpp")

# 2. 정규식
CLASS_STRUCT_PATTERN = re.compile(r'REFLECT_(CLASS|STRUCT)\s*\(\s*(\w+)\s*\)')
PROPERTY_PATTERN = re.compile(r'PROPERTY\s*\(.*?\)\s+([a-zA-Z0-9_:<, >\*&]+)\s+(\w+)(?:[^;]*?);', re.DOTALL)

# 3. 타입 매핑
TYPE_MAP = {
    "int8": "Int8", "int16": "Int16", "int32": "Int32", "int64": "Int64", "long long": "Int64",
    "uint8": "UInt8", "uint16": "UInt16", "uint32": "UInt32", "uint64": "UInt64", "unsigned long long": "UInt64", "size_t": "UInt64",
    "f32": "F32", "float": "F32", "double": "Double",
    "bool": "Bool",
    "vec2": "Vector2", "glm::vec2": "Vector2",
    "vec3": "Vector3", "glm::vec3": "Vector3",
    "vec4": "Vector4", "glm::vec4": "Vector4",
    "quat": "Quaternion", "glm::quat": "Quaternion", "Quaternion": "Quaternion",
    "string": "String", "std::string": "String",
    "wstring": "String", "std::wstring": "String"
}

def parse_template_args(type_str):
    start = type_str.find('<')
    end = type_str.rfind('>')
    if start == -1 or end == -1:
        return []
    content = type_str[start+1:end]
    return [x.strip() for x in content.split(',')]

def get_property_type(raw_type, var_name):
    if raw_type.endswith("*"):
        return "Object", raw_type.rstrip("*").strip()
    if raw_type in TYPE_MAP:
        return TYPE_MAP[raw_type], raw_type
    if raw_type.startswith("E"):
        if "Flag" in raw_type or "Mask" in raw_type or "Flag" in var_name or "Mask" in var_name:
            return "BitFlag", raw_type
        else:
            return "Enum", raw_type
    return "Struct", raw_type

def generate_reflection_code():
    print(f"[Reflection] Start Generating... Target: {OUTPUT_FILE}")
    
    generated_body = ""
    parsed_files = []

    for root, dirs, files in os.walk(ENGINE_SOURCE_DIR):
        for file in files:
            if file.endswith(".h") and file != "ReflectionMacro.h":
                filepath = os.path.join(root, file)
                content = ""
                try:
                    with open(filepath, 'r', encoding='utf-8') as f: content = f.read()
                except:
                    try: 
                        with open(filepath, 'r', encoding='cp949') as f: content = f.read()
                    except: continue

                if "REFLECT_CLASS" in content or "REFLECT_STRUCT" in content:
                    matches = CLASS_STRUCT_PATTERN.finditer(content)
                    
                    for match in matches:
                        class_name = match.group(2)
                        properties = PROPERTY_PATTERN.findall(content)

                        code_block = f"\n// Class: {class_name}\n"
                        code_block += f"BEGIN_REFLECT({class_name})\n"
                        
                        if properties:
                            for type_str, var_name in properties:
                                full_type = re.sub(r'\s+', ' ', type_str.strip())
                                raw_type = full_type.replace("std::", "").replace("Engine::", "").strip()
                                
                                # [컨테이너 처리]
                                if raw_type.startswith(("vector<", "deque<", "list<", "set<", "unordered_set<", "map<", "unordered_map<")):
                                    args = parse_template_args(raw_type)
                                    
                                    # 1. Map (인자 2개)
                                    if raw_type.startswith(("map", "unordered_map")):
                                        key_t = args[0] if len(args)>0 else "void"
                                        val_t = args[1] if len(args)>1 else "void"
                                        # REFLECT_MAP(변수, 전체타입, 키타입, 값타입, 값타입문자열)
                                        code_block += f'    REFLECT_MAP({var_name}, {full_type}, {key_t}, {val_t}, "{val_t}")\n'

                                    # 2. List (인자 1개) - ListAccessor는 FullType 필요 없음
                                    elif raw_type.startswith("list"):
                                        inner_t = args[0] if args else "void"
                                        # REFLECT_LIST(변수, 내부타입, 내부타입문자열)
                                        code_block += f'    REFLECT_LIST({var_name}, {inner_t}, "{inner_t}")\n'

                                    # 3. Set (인자 1개 + FullType)
                                    elif raw_type.startswith(("set", "unordered_set")):
                                        inner_t = args[0] if args else "void"
                                        # REFLECT_SET(변수, 전체타입, 내부타입, 내부타입문자열)
                                        code_block += f'    REFLECT_SET({var_name}, {full_type}, {inner_t}, "{inner_t}")\n'

                                    # 4. Vector/Deque (Linear)
                                    else:
                                        inner_t = args[0] if args else "void"
                                        # REFLECT_VECTOR(변수, 전체타입, 내부타입, 내부타입문자열)
                                        code_block += f'    REFLECT_VECTOR({var_name}, {full_type}, {inner_t}, "{inner_t}")\n'

                                # [일반 변수 처리]
                                else:
                                    prop_type, type_name = get_property_type(raw_type)
                                    if prop_type == "BitFlag":
                                        code_block += f'    REFLECT_BITFLAG({var_name}, Engine::EPropertyType::{prop_type}, "{type_name}")\n'
                                    else:
                                        code_block += f'    REFLECT_PROPERTY({var_name}, Engine::EPropertyType::{prop_type}, "{type_name}")\n'
                        
                        code_block += "END_REFLECT()\n"
                        generated_body += code_block
                        parsed_files.append(f"{class_name}")

    final_output = f"""// Auto-generated by ReflectionGenerator.py
#include "Core/Public/Engine_Includes.h"
#include "Core/Public/Reflection/ReflectionMacro.h"
#include "Core/Public/Reflection/ContainerReflection.h"

// [헤더 파일 자동 포함]
#include "Components/Public/Components.h" 
#include "Scene/Public/GameObject.h"

namespace Engine {{
{generated_body}
}}
"""
    try:
        os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
            f.write(final_output)
        print(f"[Reflection] Success! Generated for: {', '.join(parsed_files)}")
    except Exception as e:
        print(f"[Reflection] Error writing file: {e}")

if __name__ == "__main__":
    generate_reflection_code()