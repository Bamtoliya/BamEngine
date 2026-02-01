import os
import re
import sys

# 1. 경로 설정
ENGINE_SOURCE_DIR = "Engine/Source"
OUTPUT_FILE = os.path.abspath("Engine/Source/Core/Private/Reflection/Reflection.gen.cpp")

# 2. 정규식
CLASS_STRUCT_PATTERN = re.compile(r'REFLECT_(CLASS|STRUCT)\s*\(\s*(\w+)\s*\)')
PROPERTY_PATTERN = re.compile(r'PROPERTY\s*\((.*?)\)\s+([a-zA-Z0-9_:<, >\*&]+)\s+(\w+)(?:[^;]*?);', re.DOTALL)
ENUM_PATTERN = re.compile(r'ENUM\s*\((.*?)\)\s*enum\s+(?:class\s+)?(\w+)', re.DOTALL)

# 3. 타입 매핑 (Matrix 추가됨)
TYPE_MAP = {
    "int8": "Int8", "int16": "Int16", "int32": "Int32", "int64": "Int64", "long long": "Int64",
    "uint8": "UInt8", "uint16": "UInt16", "uint32": "UInt32", "uint64": "UInt64", "unsigned long long": "UInt64", "size_t": "UInt64",
    "f32": "F32", "float": "F32", "double": "Double",
    "bool": "Bool",
    "vec2": "Vector2", "glm::vec2": "Vector2",
    "vec3": "Vector3", "glm::vec3": "Vector3",
    "vec4": "Vector4", "glm::vec4": "Vector4",
    "mat3": "Matrix3", "glm::mat3": "Matrix3",
    "mat4": "Matrix4", "glm::mat4": "Matrix4", "Matrix": "Matrix4",
    "quat": "Quaternion", "glm::quat": "Quaternion", "Quaternion": "Quaternion",
    "string": "String", "std::string": "String",
    "wstring": "String", "std::wstring": "String"
}

def parse_template_args(type_str):
    start = type_str.find('<')
    end = type_str.rfind('>')
    if start == -1 or end == -1: return []
    content = type_str[start+1:end]
    return [x.strip() for x in content.split(',')]

def get_property_type(raw_type, var_name):
    if raw_type.endswith("*"): return "Object", raw_type.rstrip("*").strip()
    if raw_type in TYPE_MAP: return TYPE_MAP[raw_type], raw_type
    
    # Enum / BitFlag 판별
    if raw_type.startswith("E"):
        if "Flag" in raw_type or "Mask" in raw_type or "Flag" in var_name or "Mask" in var_name:
            return "BitFlag", raw_type
        else:
            return "Enum", raw_type
            
    return "Struct", raw_type

def mask_comments_and_strings(content):
    masked = re.sub(r'"(\\"|[^"])*?"', lambda m: " " * len(m.group(0)), content)
    masked = re.sub(r"'(\\'|[^'])*?'", lambda m: " " * len(m.group(0)), masked)
    masked = re.sub(r'/\*.*?\*/', lambda m: " " * len(m.group(0)), masked, flags=re.DOTALL)
    masked = re.sub(r'//.*', lambda m: " " * len(m.group(0)), masked)
    return masked

def find_scope_content(full_content, match_index):
    masked = mask_comments_and_strings(full_content)
    stack = []
    scopes = [] 
    for i, char in enumerate(masked):
        if char == '{': stack.append(i)
        elif char == '}':
            if stack:
                start = stack.pop()
                scopes.append((start, i))
    best_scope = None
    min_len = float('inf')
    for start, end in scopes:
        if start < match_index < end:
            length = end - start
            if length < min_len:
                min_len = length
                best_scope = (start, end)
    if best_scope: return full_content[best_scope[0]:best_scope[1]]
    return full_content

def find_next_scope_content(full_content, start_search_index):
    masked = mask_comments_and_strings(full_content)
    open_brace_idx = masked.find('{', start_search_index)
    if open_brace_idx == -1: return ""
    stack_count = 0
    for i in range(open_brace_idx, len(masked)):
        char = masked[i]
        if char == '{': stack_count += 1
        elif char == '}':
            stack_count -= 1
            if stack_count == 0: return full_content[open_brace_idx+1 : i]
    return ""

def generate_reflection_code():
    print(f"[Reflection] Start Generating... Target: {OUTPUT_FILE}")
    
    generated_class_body = ""  # 클래스/구조체용
    generated_enum_body = ""   # 이넘 등록 함수 내부용
    
    parsed_files = []
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

                rel_path = os.path.relpath(filepath, ENGINE_SOURCE_DIR).replace("\\", "/")

                # -----------------------------------------------------------
                # ENUM 파싱 로직 (매크로 사용 버전으로 변경)
                # -----------------------------------------------------------
                if "ENUM" in content:
                    enum_matches = ENUM_PATTERN.finditer(content)
                    has_enum = False
                    
                    for match in enum_matches:
                        enum_name = match.group(2)
                        enum_body = find_next_scope_content(content, match.end())
                        
                        clean_body = re.sub(r'//.*', '', enum_body)
                        clean_body = re.sub(r'/\*.*?\*/', '', clean_body, flags=re.DOTALL)
                        
                        entries_code = ""
                        items = [x.strip() for x in clean_body.split(',')]
                        
                        for item in items:
                            if not item: continue
                            if '=' in item: name = item.split('=')[0].strip()
                            else: name = item.strip()
                            
                            if name and re.match(r'^\w+$', name):
                                # [수정됨] 매크로 형식으로 생성
                                entries_code += f'    REFLECT_ENUM_ENTRY({enum_name}, {name})\n'
                        
                        if entries_code:
                            has_enum = True
                            # [수정됨] BEGIN / END 매크로로 감싸기
                            code_block = f"// Enum: {enum_name}\n"
                            code_block += f"BEGIN_ENUM_REFLECT({enum_name})\n"
                            code_block += entries_code
                            code_block += f"END_ENUM_REFLECT({enum_name})\n"
                            
                            generated_enum_body += code_block
                            parsed_files.append(enum_name)
                    
                    if has_enum:
                        included_headers.add(rel_path)

                # -----------------------------------------------------------
                # CLASS / STRUCT 파싱 로직 (Matrix 자동 감지 포함)
                # -----------------------------------------------------------
                if "REFLECT_CLASS" in content or "REFLECT_STRUCT" in content:
                    matches = CLASS_STRUCT_PATTERN.finditer(content)
                    has_class = False
                    for match in matches:
                        type_keyword = match.group(1) 
                        type_label = "Struct" if type_keyword == "STRUCT" else "Class"
                        class_name = match.group(2)
                        scope_content = find_scope_content(content, match.start())
                        properties = PROPERTY_PATTERN.findall(scope_content)

                        code_block = f"\n// {type_label}: {class_name}\n"
                        code_block += f"BEGIN_REFLECT({class_name})\n"
                        
                        if properties:
                            has_class = True
                            for attr_str, type_str, var_name in properties:
                                attributes = attr_str.strip().replace('\n', ' ')
                                if attributes: attributes = f", {attributes}"
                                full_type = re.sub(r'\s+', ' ', type_str.strip())
                                raw_type = full_type.replace("std::", "").replace("Engine::", "").strip()
                                
                                # 컨테이너 처리
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
                                    # [수정됨] TYPE_MAP에 Matrix가 추가되어 get_property_type이 'Matrix'를 반환함
                                    prop_type, type_name = get_property_type(raw_type, var_name)
                                    
                                    if prop_type == "BitFlag":
                                        code_block += f'    REFLECT_BITFLAG({var_name}, Engine::EPropertyType::{prop_type}, "{type_name}"{attributes})\n'
                                    else:
                                        # Matrix 등 일반 속성
                                        code_block += f'    REFLECT_PROPERTY({var_name}, Engine::EPropertyType::{prop_type}, "{type_name}"{attributes})\n'
                        
                        code_block += "END_REFLECT()\n"
                        generated_class_body += code_block
                        parsed_files.append(f"{class_name}")
                    if has_class:
                        included_headers.add(rel_path)

    headers_code = ""
    for header in sorted(included_headers):
        headers_code += f'#include "{header}"\n'

    # [최종 파일 생성]
    final_output = f"""#pragma once
// Auto-generated by ReflectionGenerator.py
#include "Core/Public/Engine_Includes.h"
#include "Core/Public/Reflection/ReflectionMacro.h"
#include "Core/Public/Reflection/ContainerReflection.h"

// [헤더 파일 자동 포함]
{headers_code}

namespace Engine {{

// [Explicit Initialization Function for Enums]
void InitEnumReflection()
{{
{generated_enum_body}
}}

// [Class/Struct Property Registration]
{generated_class_body}
}}
"""
    try:
        os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f: f.write(final_output)
        print(f"[Reflection] Success! Generated for: {', '.join(parsed_files)}")
    except Exception as e: print(f"[Reflection] Error writing file: {e}")

if __name__ == "__main__":
    generate_reflection_code()