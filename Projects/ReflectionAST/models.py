from dataclasses import dataclass, field
from typing import List

@dataclass
class PropertyInfo:
    name: str
    type_name: str
    attributes: str # "EDITABLE, ONCHANGED(...)" 등의 내용 저장

@dataclass
class FunctionInfo:
    name: str
    attributes: str
    return_type: str
    parameter_types: List[str] = field(default_factory=list)
    is_const: bool = False
    is_static: bool = False

@dataclass
class EnumEntryInfo:
    name: str
    value: int

@dataclass
class EnumInfo:
    name: str
    attributes: str
    entries: List[EnumEntryInfo] = field(default_factory=list)

@dataclass
class ClassInfo:
    name: str
    header_path: str
    namespace: str = ""
    has_reflect_macro: bool = False
    properties: List[PropertyInfo] = field(default_factory=list)
    functions: List[FunctionInfo] = field(default_factory=list)
    attributes: str = ""