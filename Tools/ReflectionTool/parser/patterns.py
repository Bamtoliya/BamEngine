import re


CLASS_STRUCT_PATTERN = re.compile(
    r'(CLASS|STRUCT)\s*\([^)]*\)\s*(?:class|struct)\s+(?:[A-Z0-9_]+\s+)*(?!abstract\b)(\w+)'
)

PROPERTY_PATTERN = re.compile(
    r'PROPERTY\s*\((.*?)\)\s+([a-zA-Z0-9_:<, >\*&]+)\s+(\w+)(?:[^;]*?);',
    re.DOTALL,
)

ENUM_PATTERN = re.compile(
    r'ENUM\s*\((.*?)\)\s*enum\s+(?:class\s+)?(\w+)',
    re.DOTALL,
)

BITMASK_ENUM_PATTERN = re.compile(
    r'ENABLE_BITMASK_OPERATORS\s*\(\s*([a-zA-Z_]\w*(?:::\w+)*)\s*\)'
)

FUNCTION_PATTERN = re.compile(
    r'FUNCTION\s*\(([^)]*)\)\s+'
    r'(?:(?:virtual|static|inline|constexpr|explicit)\s+)*'
    r'([\w:~<>\*&,\s]+?)\s+'
    r'(\w+)\s*\(([^)]*)\)\s*(const)?\s*(?:override\s*)?(?:final\s*)?;',
    re.DOTALL,
)