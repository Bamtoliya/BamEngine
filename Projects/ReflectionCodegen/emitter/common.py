import re

from config.job_schema import get_active_profile


TYPE_MAP = {
    "int8": "Int8",
    "int16": "Int16",
    "int32": "Int32",
    "int64": "Int64",
    "int": "Int32",

    "uint8": "UInt8",
    "uint16": "UInt16",
    "uint32": "UInt32",
    "uint64": "UInt64",
    "uint": "UInt32",

    "f32": "Float32",
    "float": "Float32",
    "f64": "Float64",
    "double": "Float64",

    "bool": "Bool",

    "string": "String",
    "std::string": "String",
    "String": "String",

    "wstring": "WString",
    "std::wstring": "WString",
    "WString": "WString",
    "Wstring": "WString",

    "vec2": "UserDefined",
    "glm::vec2": "UserDefined",
    "Vector2": "UserDefined",

    "vec3": "UserDefined",
    "glm::vec3": "UserDefined",
    "Vector3": "UserDefined",

    "vec4": "UserDefined",
    "glm::vec4": "UserDefined",
    "Vector4": "UserDefined",

    "quat": "UserDefined",
    "glm::quat": "UserDefined",
    "Quaternion": "UserDefined",

    "mat3": "UserDefined",
    "glm::mat3": "UserDefined",
    "Matrix3": "UserDefined",

    "mat4": "UserDefined",
    "glm::mat4": "UserDefined",
    "Matrix4": "UserDefined",
    "Matrix": "UserDefined",

    "void": "None",
}


def normalize_cpp_type(type_str: str) -> str:
    return re.sub(r"\s+", " ", type_str.strip())


def normalize_type_name(type_name: str) -> str:
    text = normalize_cpp_type(type_name)

    for namespace in get_active_profile().stripped_namespaces:
        text = text.replace(f"{namespace}::", "")

    return text.strip()


def get_unqualified_name(type_name: str) -> str:
    normalized = normalize_type_name(type_name)
    return normalized.split("::")[-1]


def parse_template_args(type_str: str) -> list[str]:
    start = type_str.find("<")
    end = type_str.rfind(">")
    if start == -1 or end == -1:
        return []

    content = type_str[start + 1:end]
    parts: list[str] = []
    current = ""
    bracket_level = 0

    for char in content:
        if char == "<":
            bracket_level += 1
        elif char == ">":
            bracket_level -= 1

        if char == "," and bracket_level == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char

    parts.append(current.strip())
    return parts


def _is_resource_handle_type(raw_type: str) -> bool:
    for template_name in get_active_profile().resource_handle_templates:
        if raw_type.startswith(f"{template_name}<"):
            return True
    return False


def get_property_type(raw_type: str, bitflag_enums: set[str] | None = None) -> tuple[str, str]:
    raw_type = normalize_type_name(raw_type)

    if _is_resource_handle_type(raw_type):
        return "ResourceHandle", raw_type

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