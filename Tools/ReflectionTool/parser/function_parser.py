import re

from model import FunctionQualifiers, ReflectedFunction, ReflectedParameter

from .patterns import FUNCTION_PATTERN


def normalize_cpp_type(type_str: str) -> str:
    return re.sub(r'\s+', ' ', type_str.strip())


def parse_function_qualifiers(suffix_raw: str) -> FunctionQualifiers:
    suffix = normalize_cpp_type(suffix_raw)

    is_const = bool(re.search(r'(?<!\w)const(?!\w)', suffix))
    is_volatile = bool(re.search(r'(?<!\w)volatile(?!\w)', suffix))
    is_noexcept = bool(re.search(r'(?<!\w)noexcept(?:\s*\([^)]*\))?(?!\w)', suffix))

    ref_qualifier = ""
    if re.search(r'&&', suffix):
        ref_qualifier = "&&"
    elif re.search(r'(?<!&)&(?!&)', suffix):
        ref_qualifier = "&"

    return FunctionQualifiers(
        is_const=is_const,
        is_volatile=is_volatile,
        ref_qualifier=ref_qualifier,
        is_noexcept=is_noexcept,
    )


def parse_param_list(params_str: str) -> list[ReflectedParameter]:
    params_str = params_str.strip()
    if not params_str or params_str == "void":
        return []

    parts: list[str] = []
    depth = 0
    current = ""

    for char in params_str:
        if char == "<":
            depth += 1
        elif char == ">":
            depth -= 1

        if char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char

    if current.strip():
        parts.append(current.strip())

    params: list[ReflectedParameter] = []

    for part in parts:
        part = re.split(r'=', part)[0].strip()
        name_match = re.search(r'(\w+)\s*$', part)
        if not name_match:
            continue

        name = name_match.group(1)
        cpp_type = part[:name_match.start()].strip()

        if cpp_type and cpp_type not in ("const", "volatile"):
            params.append(
                ReflectedParameter(
                    cpp_type=cpp_type,
                    name=name,
                )
            )

    return params


def parse_functions(
    scope_content: str,
    owner_name: str,
    owner_qualified_name: str,
) -> list[ReflectedFunction]:
    functions: list[ReflectedFunction] = []

    for _attrs, return_type_raw, function_name, params_str, suffix_raw in FUNCTION_PATTERN.findall(scope_content):
        qualifiers = parse_function_qualifiers(suffix_raw)
        params = parse_param_list(params_str)

        function = ReflectedFunction(
            name=function_name.strip(),
            signature="",
            owner_name=owner_name,
            owner_qualified_name=owner_qualified_name,
            return_type_raw=normalize_cpp_type(return_type_raw),
            params=params,
            qualifiers=qualifiers,
        )
        function.signature = function.make_signature_text()
        functions.append(function)

    return functions