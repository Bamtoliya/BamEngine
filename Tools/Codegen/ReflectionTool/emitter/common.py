import re


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
    "f32": "F32",
    "float": "F32",
    "double": "Double",
    "f64": "Double",
    "bool": "Bool",
    "vec2": "Vector2",
    "glm::vec2": "Vector2",
    "Vector2": "Vector2",
    "vec3": "Vector3",
    "glm::vec3": "Vector3",
    "Vector3": "Vector3",
    "vec4": "Vector4",
    "glm::vec4": "Vector4",
    "Vector4": "Vector4",
    "mat3": "Matrix3",
    "glm::mat3": "Matrix3",
    "mat4": "Matrix4",
    "glm::mat4": "Matrix4",
    "Matrix": "Matrix4",
    "quat": "Quaternion",
    "glm::quat": "Quaternion",
    "Quaternion": "Quaternion",
    "string": "String",
    "std::string": "String",
    "String": "String",
    "wstring": "Wstring",
    "std::wstring": "Wstring",
    "Wstring": "Wstring",
    "void": "None",
}


def normalize_cpp_type(type_str: str) -> str:
    return re.sub(r'\s+', ' ', type_str.strip())


def normalize_type_name(type_name: str) -> str:
    return type_name.replace("std::", "").replace("Engine::", "").strip()


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


def get_property_type(raw_type: str, bitflag_enums: set[str] | None = None) -> tuple[str, str]:
    raw_type = normalize_type_name(raw_type)

    if raw_type.startswith("ResourceHandle<"):
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