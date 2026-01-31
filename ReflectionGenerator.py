import os
import re
import sys

# 1. 경로 설정
ENGINE_SOURCE_DIR = "Engine/Source"
OUTPUT_FILE = os.path.abspath("Engine/Source/Core/Private/Reflection/Reflection.gen.cpp")

# 2. 정규식
CLASS_STRUCT_PATTERN = re.compile(r'REFLECT_(CLASS|STRUCT)\s*\(\s*(\w+)\s*\)')
PROPERTY_PATTERN = re.compile(r'PROPERTY\s*\((.*?)\)\s+([a-zA-Z0-9_:<, >\*&]+)\s+(\w+)(?:[^;]*?);', re.DOTALL)

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

def find_scope_content(full_content, match_index):
    """
    REFLECT_ 매크로가 위치한 가장 가까운 중괄호 { } 범위를 찾아 그 내용을 반환합니다.
    주석이나 문자열 내부의 중괄호로 인한 오류를 방지하기 위해 마스킹 처리를 포함합니다.
    """
    # 1. 주석 및 문자열 마스킹 (중괄호 카운팅 방해 요소 제거)
    # 문자열("...", '...')을 공백으로 치환
    masked = re.sub(r'"(\\"|[^"])*?"', lambda m: " " * len(m.group(0)), full_content)
    masked = re.sub(r"'(\\'|[^'])*?'", lambda m: " " * len(m.group(0)), masked)
    # 블록 주석(/* ... */)을 공백으로 치환
    masked = re.sub(r'/\*.*?\*/', lambda m: " " * len(m.group(0)), masked, flags=re.DOTALL)
    # 라인 주석(// ...)을 공백으로 치환
    masked = re.sub(r'//.*', lambda m: " " * len(m.group(0)), masked)

    # 2. 모든 중괄호 쌍 찾기
    stack = []
    scopes = [] # (start, end) 튜플 리스트
    
    for i, char in enumerate(masked):
        if char == '{':
            stack.append(i)
        elif char == '}':
            if stack:
                start = stack.pop()
                scopes.append((start, i))

    # 3. match_index(매크로 위치)를 포함하는 가장 작은(최하위) 범위 찾기
    best_scope = None
    min_len = float('inf')

    for start, end in scopes:
        if start < match_index < end:
            length = end - start
            if length < min_len:
                min_len = length
                best_scope = (start, end)
    
    if best_scope:
        # 실제 원본 콘텐츠에서 해당 범위만 추출
        return full_content[best_scope[0]:best_scope[1]]
    
    return full_content # 범위를 못 찾으면 전체 반환 (Fallback)

def generate_reflection_code():
    print(f"[Reflection] Start Generating... Target: {OUTPUT_FILE}")
    
    generated_body = ""
    parsed_files = []
    
    # [추가 1] 헤더 파일을 수집할 집합(Set) 생성 (중복 방지)
    included_headers = set()

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
                    # [추가 2] 해당 파일의 경로를 수집
                    # Engine/Source를 기준으로 상대 경로 계산 및 역슬래시(\)를 슬래시(/)로 변경
                    rel_path = os.path.relpath(filepath, ENGINE_SOURCE_DIR).replace("\\", "/")
                    included_headers.add(rel_path)

                    matches = CLASS_STRUCT_PATTERN.finditer(content)
                    
                    for match in matches:
                        type_keyword = match.group(1) # CLASS or STRUCT
                        type_label = "Struct" if type_keyword == "STRUCT" else "Class"
                        class_name = match.group(2)
                        
                        scope_content = find_scope_content(content, match.start())
                        properties = PROPERTY_PATTERN.findall(scope_content)

                        code_block = f"\n// {type_label}: {class_name}\n"
                        code_block += f"BEGIN_REFLECT({class_name})\n"
                        
                        if properties:
                            for attr_str, type_str, var_name in properties:

                                attributes = attr_str.strip().replace('\n', ' ')

                                if attributes:
                                    attributes = f", {attributes}"

                                full_type = re.sub(r'\s+', ' ', type_str.strip())
                                raw_type = full_type.replace("std::", "").replace("Engine::", "").strip()
                                
                                if raw_type.startswith(("vector<", "deque<", "list<", "set<", "unordered_set<", "map<", "unordered_map<")):
                                    args = parse_template_args(raw_type)
                                    if raw_type.startswith(("map", "unordered_map")):
                                        key_t = args[0] if len(args)>0 else "void"
                                        val_t = args[1] if len(args)>1 else "void"
                                        code_block += f'    REFLECT_MAP({var_name}, {full_type}, {key_t}, {val_t}, "{val_t}"{attributes})\n'
                                    elif raw_type.startswith("list"):
                                        inner_t = args[0] if args else "void"
                                        code_block += f'    REFLECT_LIST({var_name}, {inner_t}, "{inner_t}"{attributes})\n'
                                    elif raw_type.startswith(("set", "unordered_set")):
                                        inner_t = args[0] if args else "void"
                                        code_block += f'    REFLECT_SET({var_name}, {full_type}, {inner_t}, "{inner_t}"{attributes})\n'
                                    else:
                                        inner_t = args[0] if args else "void"
                                        code_block += f'    REFLECT_VECTOR({var_name}, {full_type}, {inner_t}, "{inner_t}"{attributes})\n'
                                else:
                                    # 인자 개수 수정 적용
                                    prop_type, type_name = get_property_type(raw_type, var_name)
                                    if prop_type == "BitFlag":
                                        code_block += f'    REFLECT_BITFLAG({var_name}, Engine::EPropertyType::{prop_type}, "{type_name}"{attributes})\n'
                                    else:
                                        code_block += f'    REFLECT_PROPERTY({var_name}, Engine::EPropertyType::{prop_type}, "{type_name}"{attributes})\n'

                        code_block += "END_REFLECT()\n"
                        generated_body += code_block
                        parsed_files.append(f"{class_name}")

    # [추가 3] 수집된 헤더들을 문자열로 변환
    # 정렬(sorted)하여 매번 순서가 바뀌지 않도록 함
    headers_code = ""
    for header in sorted(included_headers):
        headers_code += f'#include "{header}"\n'

    # [추가 4] final_output에 headers_code 삽입
    final_output = f"""#pragma once
// Auto-generated by ReflectionGenerator.py
#include "Core/Public/Engine_Includes.h"
#include "Core/Public/Reflection/ReflectionMacro.h"
#include "Core/Public/Reflection/ContainerReflection.h"

// [헤더 파일 자동 포함]
{headers_code}

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