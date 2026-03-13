import os
import re
from collections import Counter

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
BITMASK_ENUM_PATTERN = re.compile(
    r'ENABLE_BITMASK_OPERATORS\s*\(\s*([a-zA-Z_]\w*(?:::\w+)*)\s*\)'
)
FUNCTION_PATTERN = re.compile(
    r'FUNCTION\s*\(([^)]*)\)\s+'
    r'(?:(?:virtual|static|inline|constexpr|explicit)\s+)*'
    r'([\w:~<>\*&,\s]+?)\s+'
    r'(\w+)\s*\(([^)]*)\)\s*(const)?\s*(?:override\s*)?(?:final\s*)?;',
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

def normalize_cpp_type(type_str: str) -> str:
    return re.sub(r'\s+', ' ', type_str.strip())

def make_overload_suffix(params: list[tuple[str, str]]) -> str:
    if not params:
        return "void"

    parts = []
    for p_type_str, _ in params:
        clean = normalize_cpp_type(p_type_str)
        clean = clean.replace("const ", "")
        clean = clean.replace("&", "")
        clean = clean.replace("*", "Ptr")
        clean = clean.replace("::", "_")
        clean = re.sub(r'[^a-zA-Z0-9_]', '_', clean)
        parts.append(clean)

    return "_".join(parts) if parts else "void"

def make_signature_text(func_name: str, params: list[tuple[str, str]]) -> str:
    if not params:
        return f"{func_name}()"
    param_types = ", ".join(normalize_cpp_type(p_type_str) for p_type_str, _ in params)
    return f"{func_name}({param_types})"

def make_function_pointer_cast(
    class_name: str,
    func_name: str,
    return_type_raw: str,
    params: list[tuple[str, str]],
    qualifiers: dict[str, object]
) -> str:
    ret = normalize_cpp_type(return_type_raw)

    if not params:
        args = ""
    else:
        args = ", ".join(normalize_cpp_type(p_type_str) for p_type_str, _ in params)

    suffix_parts: list[str] = []

    if qualifiers["is_const"]:
        suffix_parts.append("const")
    if qualifiers["is_volatile"]:
        suffix_parts.append("volatile")
    if qualifiers["ref_qualifier"]:
        suffix_parts.append(qualifiers["ref_qualifier"])
    if qualifiers["is_noexcept"]:
        suffix_parts.append("noexcept")

    qualifier_suffix = ""
    if suffix_parts:
        qualifier_suffix = " " + " ".join(suffix_parts)

    return f"static_cast<{ret} ({class_name}::*)({args}){qualifier_suffix}>(&{class_name}::{func_name})"

def normalize_type_name(type_name: str) -> str:
    return type_name.replace("std::", "").replace("Engine::", "").strip()

def get_unqualified_name(type_name: str) -> str:
    normalized = normalize_type_name(type_name)
    return normalized.split("::")[-1]

def parse_function_qualifiers(suffix_raw: str) -> dict[str, object]:
    suffix = normalize_cpp_type(suffix_raw)

    is_const = bool(re.search(r'(?<!\w)const(?!\w)', suffix))
    is_volatile = bool(re.search(r'(?<!\w)volatile(?!\w)', suffix))
    is_noexcept = bool(re.search(r'(?<!\w)noexcept(?:\s*\([^)]*\))?(?!\w)', suffix))

    ref_qualifier = ""
    if re.search(r'&&', suffix):
        ref_qualifier = "&&"
    elif re.search(r'(?<!&)&(?!&)', suffix):
        ref_qualifier = "&"

    return {
        "is_const": is_const,
        "is_volatile": is_volatile,
        "ref_qualifier": ref_qualifier,
        "is_noexcept": is_noexcept,
    }

def collect_bitflag_enums(content: str) -> set[str]:
    result = set()
    for match in BITMASK_ENUM_PATTERN.finditer(content):
        full_name = match.group(1).strip()
        result.add(normalize_type_name(full_name))
        result.add(get_unqualified_name(full_name))
    return result

def get_property_type(raw_type: str, bitflag_enums: set[str] | None = None, var_name: str = "") -> tuple[str, str]:
    raw_type = normalize_type_name(raw_type)

    if raw_type.endswith("*"):
        return "Object", raw_type.rstrip("*").strip()

    if raw_type in TYPE_MAP:
        return TYPE_MAP[raw_type], raw_type

    if bitflag_enums:
        raw_unqualified = get_unqualified_name(raw_type)
        if raw_type in bitflag_enums or raw_unqualified in bitflag_enums:
            return "BitFlag", raw_type

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
    params_str = params_str.strip()
    if not params_str or params_str == 'void':
        return []

    parts, depth, current = [], 0, ""
    for c in params_str:
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += c
    if current.strip():
        parts.append(current.strip())

    result = []
    for part in parts:
        part = re.split(r'=', part)[0].strip()
        name_match = re.search(r'(\w+)\s*$', part)
        if not name_match:
            continue

        name = name_match.group(1)
        type_str = part[:name_match.start()].strip()

        if type_str and type_str not in ('const', 'volatile'):
            result.append((type_str, name))

    return result

# ==========================================================
# 프로퍼티 추출
#   반환값:
#     decl_code  - 메타데이터 배열 / DECLARE_CONTAINER_INFO 등 선언 문자열
#     props_list - REFLECT_PROPERTY / REFLECT_CONTAINER_PROPERTY 호출 목록
# ==========================================================

def extract_properties(class_name: str, properties: list, bitflag_enums: set[str]) -> tuple[str, list[str]]:
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
        raw_type  = normalize_type_name(full_type)

        # ── 컨테이너 타입 ────────────────────────────────────────────────
        if raw_type.lower().startswith(
            ("vector<","deque<","list<","set<","unordered_set<","map<","unordered_map<")
        ):
            args = parse_template_args(raw_type)

            if "map" in raw_type.lower():
                key_t    = args[0] if len(args) > 0 else "void"
                val_t    = args[1] if len(args) > 1 else "void"
                key_enum = f"Engine::EPropertyType::{get_property_type(key_t, bitflag_enums)[0]}"
                val_enum = f"Engine::EPropertyType::{get_property_type(val_t, bitflag_enums)[0]}"
                accessor = f"Engine::MapAccessor<{full_type}>::Get()"
                decl_code += (
                    f"DECLARE_MAP_INFO({class_name}, {var_name}, "
                    f'"{key_t}", {key_enum}, "{val_t}", {val_enum}, {accessor})\n'
                )
                prop_enum = "Engine::EPropertyType::Map"

            else:
                inner_t    = args[0] if args else "void"
                inner_enum = f"Engine::EPropertyType::{get_property_type(inner_t, bitflag_enums)[0]}"

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
            p_type, t_name = get_property_type(raw_type, bitflag_enums, var_name)
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

    name_counts = Counter(func_name for _, _, func_name, _, _ in functions)

    for _func_attrs, return_type_raw, func_name, params_str, suffix_raw in functions:
        qualifiers = parse_function_qualifiers(suffix_raw)

        ret = normalize_cpp_type(return_type_raw)
        for q in ('virtual', 'static', 'inline', 'constexpr', 'explicit', 'override', 'final'):
            ret = re.sub(rf'\b{q}\b', '', ret).strip()

        ret_clean = ret.replace("const", "").replace("&", "").replace("*", "").strip()
        r_type, r_name = get_property_type(ret_clean)
        r_enum = f"Engine::EPropertyType::{r_type}"

        params = parse_param_list(params_str)
        overload_suffix = make_overload_suffix(params)
        unique_func_name = f"{func_name}_{overload_suffix}"
        signature_text = make_signature_text(func_name, params)
        params_arg = "{}"

        if params:
            params_arg = f"{class_name}_{unique_func_name}_Params"
            param_code += f"DECLARE_FUNCTION_PARAMS({class_name}, {unique_func_name})\n"
            for p_type_str, _ in params:
                pt, ptn = get_property_type(
                    p_type_str.replace("const", "").replace("&", "").replace("*", "").strip()
                )
                param_code += f'\tFUNCTION_PARAM("{ptn}", Engine::EPropertyType::{pt})\n'
            param_code += "END_FUNCTION_PARAMS\n\n"

        is_overloaded = name_counts[func_name] > 1

        if is_overloaded:
            function_ptr = make_function_pointer_cast(class_name, func_name, ret, params, qualifiers)
            funcs_list.append(
                f'REFLECT_FUNCTION_OVERLOAD({class_name}, "{func_name}", "{signature_text}", {function_ptr}, "{r_name}", {r_enum}, {params_arg})'
            )
        else:
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

def parse_class_struct(content: str, rel_path: str, bitflag_enums: set[str]) -> tuple[bool, str]:
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

        raw_properties = PROPERTY_PATTERN.findall(scope_content)
        raw_functions  = FUNCTION_PATTERN.findall(scope_content)

        code_block = f"#pragma region {type_keyword}: {class_name}\n"

        prop_decl_code, props_list = extract_properties(class_name, raw_properties, bitflag_enums)
        code_block += prop_decl_code

        if not props_list:
            code_block += f"EMPTY_PROPERTIES({class_name})\n\n"
        else:
            code_block += f"BEGIN_PROPERTIES({class_name})\n"
            for p in props_list:
                code_block += f"\t{p}\n"
            code_block += "END_PROPERTIES\n\n"

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
    bitflag_enums        = set()
    header_contents      = []

    for root, dirs, files in os.walk(ENGINE_SOURCE_DIR):
        for file in files:
            if not (file.endswith(".h") and file != "ReflectionMacro.h"):
                continue

            filepath = os.path.join(root, file)
            content  = read_file_content(filepath)
            if not content:
                continue

            rel_path = os.path.relpath(filepath, ENGINE_SOURCE_DIR).replace("\\", "/")
            header_contents.append((rel_path, content))
            bitflag_enums.update(collect_bitflag_enums(content))

    for rel_path, content in header_contents:
        has_enum, enum_body = parse_enum(content, rel_path)
        if has_enum:
            generated_enum_body += enum_body
            included_headers.add(rel_path)

        has_type, class_code = parse_class_struct(content, rel_path, bitflag_enums)
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