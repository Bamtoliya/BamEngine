import os
import re

# 1. 경로 설정
ENGINE_SOURCE_DIR = "Engine/Source"
OUTPUT_FILE = os.path.abspath("Engine/Source/Core/Reflection/Private/Reflection.gen.cpp")

# 2. 정규식
CLASS_STRUCT_PATTERN = re.compile(
    r'(CLASS|STRUCT)\s*\([^)]*\)\s*(?:class|struct)\s+(?:[A-Z0-9_]+\s+)*(?!abstract\b)(\w+)'
)
PROPERTY_PATTERN = re.compile(
    r'PROPERTY\s*\((.*?)\)\s+([a-zA-Z0-9_:<, >\*&]+)\s+(\w+)(?:[^;]*?);',
    re.DOTALL
)
ENUM_PATTERN = re.compile(
    r'ENUM\s*\((.*?)\)\s*enum\s+(?:class\s+)?(\w+)',
    re.DOTALL
)
FUNCTION_PATTERN = re.compile(
    r'FUNCTION\s*\(([^)]*)\)\s+'
    r'(?:(?:virtual|static|inline|constexpr|explicit)\s+)*'
    r'([\w:~<>\*&,\s]+?)\s+'
    r'(\w+)\s*\(([^)]*)\)',
    re.DOTALL
)

# 3. 타입 매핑
TYPE_MAP = {
    "int8":  "Int8",  "int16": "Int16", "int32": "Int32", "int64": "Int64", "int": "Int32",
    "uint8": "UInt8", "uint16":"UInt16","uint32":"UInt32","uint64":"UInt64","uint":"UInt32",
    "f32": "F32", "float": "F32", "double": "Double", "f64": "Double",
    "bool": "Bool",
    "vec2": "Vector2", "glm::vec2": "Vector2", "Vector2": "Vector2",
    "vec3": "Vector3", "glm::vec3": "Vector3", "Vector3": "Vector3",
    "vec4": "Vector4", "glm::vec4": "Vector4", "Vector4": "Vector4",
    "mat3": "Matrix3", "glm::mat3": "Matrix3",
    "mat4": "Matrix4", "glm::mat4": "Matrix4", "Matrix": "Matrix4",
    "quat": "Quaternion", "glm::quat": "Quaternion", "Quaternion": "Quaternion",
    "string":  "String",  "std::string":  "String",  "String":  "String",
    "wstring": "Wstring", "std::wstring": "Wstring", "Wstring": "Wstring",
    "void": "None",
}

# ==========================================================
# 유틸
# ==========================================================

def parse_template_args(type_str: str) -> list[str]:
    start = type_str.find('<')
    end   = type_str.rfind('>')
    if start == -1 or end == -1:
        return []
    content = type_str[start+1:end]
    parts, bracket_level, current = [], 0, ""
    for char in content:
        if char == '<':   bracket_level += 1
        elif char == '>': bracket_level -= 1
        if char == ',' and bracket_level == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char
    parts.append(current.strip())
    return parts

def get_property_type(raw_type: str, var_name: str = "") -> tuple[str, str]:
    raw_type = raw_type.replace("std::", "").replace("Engine::", "").strip()
    if raw_type.endswith("*"):
        return "Object", raw_type.rstrip("*").strip()
    if raw_type in TYPE_MAP:
        return TYPE_MAP[raw_type], raw_type
    if raw_type.startswith("E") or "Enum" in raw_type:
        return "Enum", raw_type
    return "Struct", raw_type

def mask_comments_and_strings(content: str) -> str:
    content = re.sub(r'/\*.*?\*/', lambda m: " " * len(m.group(0)), content, flags=re.DOTALL)
    content = re.sub(r'//.*',      lambda m: " " * len(m.group(0)), content)
    return content

def find_class_body(full_content: str, start_index: int) -> tuple[str, int]:
    masked = mask_comments_and_strings(full_content)
    open_brace_idx = masked.find('{', start_index)
    if open_brace_idx == -1:
        return "", start_index
    stack = 0
    for i in range(open_brace_idx, len(masked)):
        if masked[i] == '{':   stack += 1
        elif masked[i] == '}':
            stack -= 1
            if stack == 0:
                return full_content[open_brace_idx+1:i], open_brace_idx
    return "", start_index

def find_parent_class(full_content: str, class_name: str, search_end_index: int) -> str:
    search_start = max(0, search_end_index - 300)
    pre_text     = mask_comments_and_strings(full_content[search_start:search_end_index])
    pattern_str  = (
        r'(class|struct)\s+(?:[A-Z0-9_]+\s+)*'
        + re.escape(class_name)
        + r'\s*(?:final|abstract)?\s*:\s*public\s+(\w+)'
    )
    match = re.search(pattern_str, pre_text)
    return match.group(2) if match else "None"

def read_file_content(filepath: str) -> str:
    for enc in ('utf-8', 'cp949'):
        try:
            with open(filepath, 'r', encoding=enc) as f:
                return f.read()
        except Exception:
            continue
    return ""

def parse_param_list(params_str: str) -> list[tuple[str, str]]:
    """'T1 n1, T2 n2' → [(type, name), ...]"""
    params_str = params_str.strip()
    if not params_str or params_str == 'void':
        return []
    parts, depth, current = [], 0, ""
    for c in params_str:
        if c == '<':   depth += 1
        elif c == '>': depth -= 1
        if c == ',' and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += c
    if current.strip():
        parts.append(current.strip())

    result = []
    for part in parts:
        part       = re.split(r'=', part)[0].strip()
        name_match = re.search(r'(\w+)\s*$', part)
        if not name_match:
            continue
        name     = name_match.group(1)
        type_str = part[:name_match.start()].strip().rstrip('*& ').strip()
        if type_str and type_str not in ('const', 'volatile'):
            result.append((type_str, name))
    return result

# ==========================================================
# 프로퍼티 추출
#   반환값:
#     decl_code  - 메타데이터 배열 / DECLARE_CONTAINER_INFO 등 선언 문자열
#     props_list - REFLECT_PROPERTY / REFLECT_CONTAINER_PROPERTY 호출 목록
# ==========================================================

def extract_properties(class_name: str, properties: list) -> tuple[str, list[str]]:
    decl_code  = ""
    props_list = []

    for attr_str, type_str, var_name in properties:
        attr_str        = attr_str.strip()
        meta_array_name = "{}"

        # ── 메타데이터 블록 ──────────────────────────────────────────────
        if attr_str:
            meta_array_name  = f"{class_name}_{var_name}_Meta"
            decl_code       += f"BEGIN_METADATA({class_name}, {var_name})\n"
            for attr in re.split(r',(?![^(]*\))', attr_str):
                decl_code += f"\t{attr.strip()}\n"
            decl_code += "END_METADATA\n\n" 

        full_type = re.sub(r'\s+', ' ', type_str.strip())
        raw_type  = full_type.replace("std::", "").replace("Engine::", "").strip()

        # ── 컨테이너 타입 ────────────────────────────────────────────────
        if raw_type.lower().startswith(
            ("vector<","deque<","list<","set<","unordered_set<","map<","unordered_map<")
        ):
            args = parse_template_args(raw_type)

            if "map" in raw_type.lower():
                key_t    = args[0] if len(args) > 0 else "void"
                val_t    = args[1] if len(args) > 1 else "void"
                key_enum = f"Engine::EPropertyType::{get_property_type(key_t)[0]}"
                val_enum = f"Engine::EPropertyType::{get_property_type(val_t)[0]}"
                accessor = f"Engine::MapAccessor<{full_type}>::Get()"
                decl_code += (
                    f"DECLARE_MAP_INFO({class_name}, {var_name}, "
                    f'"{key_t}", {key_enum}, "{val_t}", {val_enum}, {accessor})\n'
                )
                prop_enum = "Engine::EPropertyType::Map"

            else:
                inner_t    = args[0] if args else "void"
                inner_enum = f"Engine::EPropertyType::{get_property_type(inner_t)[0]}"

                if "set" in raw_type.lower():
                    accessor  = f"Engine::SetAccessor<{full_type}, {inner_t}>::Get()"
                    prop_enum = "Engine::EPropertyType::Set"
                elif raw_type.lower().startswith("list<"):
                    accessor  = f"Engine::ListAccessor<{inner_t}>::Get()"
                    prop_enum = "Engine::EPropertyType::List"
                else:
                    accessor  = f"Engine::LinearContainerAccessor<{full_type}, {inner_t}>::Get()"
                    prop_enum = "Engine::EPropertyType::Array"

                decl_code += (
                    f"DECLARE_CONTAINER_INFO({class_name}, {var_name}, "
                    f'"{inner_t}", {inner_enum}, {accessor})\n'
                )

            container_ptr = f"&{class_name}_{var_name}_ContainerData"
            props_list.append(
                f'REFLECT_CONTAINER_PROPERTY({class_name}, {var_name}, "{full_type}", '
                f'{prop_enum}, {container_ptr}, {meta_array_name})'
            )

        # ── 단순 타입 ────────────────────────────────────────────────────
        else:
            p_type, t_name = get_property_type(raw_type, var_name)
            p_enum = f"Engine::EPropertyType::{p_type}"
            props_list.append(
                f'REFLECT_PROPERTY({class_name}, {var_name}, "{t_name}", {p_enum}, {meta_array_name})'
            )

    return decl_code, props_list

# ==========================================================
# 함수 추출
#   반환값:
#     param_code - 파라미터 VariableInfo 배열 선언 문자열
#     funcs_list - REFLECT_FUNCTION 호출 목록
# ==========================================================

def extract_functions(class_name: str, functions: list) -> tuple[str, list[str]]:
    param_code = ""
    funcs_list = []

    for _func_attrs, return_type_raw, func_name, params_str in functions:
        # ── 반환 타입 정규화 ──────────────────────────────────
        ret = return_type_raw.strip()
        for q in ('virtual','static','inline','constexpr','explicit','override','final'):
            ret = re.sub(rf'\b{q}\b', '', ret).strip()

        ret_clean = ret.replace("const","").replace("&","").replace("*","").strip()
        r_type, r_name = get_property_type(ret_clean)
        r_enum = f"Engine::EPropertyType::{r_type}"

        # ── 파라미터 ─────────────────────────────────────────
        params = parse_param_list(params_str)
        params_arg = "{}"

        if params:
            params_arg = f"{class_name}_{func_name}_Params"
            param_code += f"DECLARE_FUNCTION_PARAMS({class_name}, {func_name})\n"
            for p_type_str, _ in params:
                pt, ptn = get_property_type(
                    p_type_str.replace("const","").replace("&","").replace("*","").strip()
                )
                param_code += f'\tFUNCTION_PARAM("{ptn}", Engine::EPropertyType::{pt})\n'
            param_code += "END_FUNCTION_PARAMS\n\n"

        funcs_list.append(
            f'REFLECT_FUNCTION({class_name}, {func_name}, "{r_name}", {r_enum}, {params_arg})'
        )

    return param_code, funcs_list

# ==========================================================
# 열거형 파싱
# ==========================================================

def parse_enum(content: str, rel_path: str) -> tuple[bool, str]:
    parsed_enums_code, has_enum_in_file = [], False
    if "ENUM" not in content:
        return False, ""

    for match in ENUM_PATTERN.finditer(content):
        enum_name    = match.group(2)
        enum_body, _ = find_class_body(content, match.end())
        clean_body   = re.sub(r'//.*',       '', enum_body)
        clean_body   = re.sub(r'/\*.*?\*/', '', clean_body, flags=re.DOTALL)

        entries_code = ""
        for item in [x.strip() for x in clean_body.split(',')]:
            if not item:
                continue
            name = item.split('=')[0].strip()
            if name and re.match(r'^\w+$', name):
                entries_code += f'\tREFLECT_ENUM_ENTRY({enum_name}, {name})\n'

        if entries_code:
            has_enum_in_file = True
            parsed_enums_code.append(
                f"// Enum: {enum_name}\n"
                f"BEGIN_ENUM({enum_name})\n"
                f"{entries_code}"
                f"END_ENUM_REFLECT({enum_name})\n"
            )

    return has_enum_in_file, "\n".join(parsed_enums_code)

# ==========================================================
# 클래스/구조체 파싱
# ==========================================================

def parse_class_struct(content: str, rel_path: str) -> tuple[bool, str]:
    generated_class_body, has_type_in_file = "", False
    if not any(k in content for k in ["CLASS", "STRUCT", "PROPERTY"]):
        return False, ""

    for match in list(CLASS_STRUCT_PATTERN.finditer(content)):
        type_keyword = match.group(1)
        class_name   = match.group(2)
        scope_content, brace_start_idx = find_class_body(content, match.end())

        if not scope_content:
            continue
        if not any(m in scope_content for m in ["REFLECT_CLASS", "REFLECT_STRUCT", "REFLECT_BASE"]):
            continue

        has_type_in_file = True
        parent_name = find_parent_class(content, class_name, brace_start_idx)

        # ── 원시 매치 목록 ───────────────────────────────────────────────
        raw_properties = PROPERTY_PATTERN.findall(scope_content)
        raw_functions  = FUNCTION_PATTERN.findall(scope_content)

        code_block = f"#pragma region {type_keyword}: {class_name}\n"

        # ── 프로퍼티 ─────────────────────────────────────────────────────
        prop_decl_code, props_list = extract_properties(class_name, raw_properties)
        code_block += prop_decl_code

        if not props_list:
            code_block += f"EMPTY_PROPERTIES({class_name})\n\n"
        else:
            code_block += f"BEGIN_PROPERTIES({class_name})\n"
            for p in props_list:
                code_block += f"\t{p}\n"
            code_block += "END_PROPERTIES\n\n"

        # ── 함수 ─────────────────────────────────────────────────────────
        func_param_code, funcs_list = extract_functions(class_name, raw_functions)

        if func_param_code:
            code_block += func_param_code + "\n"

        if not funcs_list:
            code_block += f"EMPTY_FUNCTIONS({class_name})\n\n"
        else:
            code_block += f"BEGIN_FUNCTIONS({class_name})\n"
            for f in funcs_list:
                code_block += f"\t{f}\n"
            code_block += "END_FUNCTIONS\n\n"

        code_block += f"IMPLEMENT_CLASS({class_name}, {parent_name})\n\n"
        code_block += f"#pragma endregion // {type_keyword}: {class_name}\n\n"
        generated_class_body += code_block

    return has_type_in_file, generated_class_body

# ==========================================================
# 메인
# ==========================================================

def generate_reflection_code():
    print(f"[Reflection] Scanning Engine Source: {ENGINE_SOURCE_DIR}")

    generated_class_body = ""
    generated_enum_body  = ""
    parsed_files_count   = 0
    included_headers     = set()

    for root, dirs, files in os.walk(ENGINE_SOURCE_DIR):
        for file in files:
            if not (file.endswith(".h") and file != "ReflectionMacro.h"):
                continue
            filepath = os.path.join(root, file)
            content  = read_file_content(filepath)
            if not content:
                continue
            rel_path = os.path.relpath(filepath, ENGINE_SOURCE_DIR).replace("\\", "/")

            has_enum, enum_body = parse_enum(content, rel_path)
            if has_enum:
                generated_enum_body += enum_body
                included_headers.add(rel_path)

            has_type, class_code = parse_class_struct(content, rel_path)
            if has_type:
                generated_class_body += class_code
                included_headers.add(rel_path)
                parsed_files_count += 1

    headers_code = "\n".join(f'#include "{h}"' for h in sorted(included_headers))

    final_output = f"""// Auto-generated by ReflectionGenerator.py
#include "Core/Public/Engine_Includes.h"
#include "Core/Reflection/Public/ReflectionMacro.h"
#include "Core/Reflection/Public/ReflectionContainers.h"

// [Header Includes]
{headers_code}

namespace Engine {{

#pragma push_macro("new")
#undef new

// ==========================================================
// [ENUM REFLECTIONS]
// ==========================================================
#pragma region EnumReflections
{generated_enum_body}
#pragma endregion

#pragma region ClassStructReflections
// ==========================================================
// [CLASS/STRUCT REFLECTIONS]
// ==========================================================
{generated_class_body}
#pragma endregion

#pragma pop_macro("new")

}} // namespace Engine
"""
    try:
        os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
            f.write(final_output)
        print(f"[Reflection] Success! Generated {parsed_files_count} types into {OUTPUT_FILE}")
    except Exception as e:
        print(f"[Reflection] Error writing file: {e}")

if __name__ == "__main__":
    generate_reflection_code()