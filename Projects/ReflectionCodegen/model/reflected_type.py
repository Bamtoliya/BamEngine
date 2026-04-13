from dataclasses import dataclass, field

from .reflected_function import ReflectedFunction
from .reflected_property import ReflectedProperty


@dataclass(slots=True)
class ReflectedType:
    kind: str
    name: str
    qualified_name: str
    namespace_parts: list[str]
    parent_name: str = ""
    parent_qualified_name: str = ""
    source_file: str = ""
    properties: list[ReflectedProperty] = field(default_factory=list)
    functions: list[ReflectedFunction] = field(default_factory=list)

    @property
    def namespace(self) -> str:
        return "::".join(self.namespace_parts)

    @property
    def short_name(self) -> str:
        return self.name

    def is_class(self) -> bool:
        return self.kind == "CLASS"

    def is_struct(self) -> bool:
        return self.kind == "STRUCT"

    def has_parent(self) -> bool:
        return bool(self.parent_qualified_name)

    def add_property(self, prop: ReflectedProperty) -> None:
        self.properties.append(prop)

    def add_function(self, func: ReflectedFunction) -> None:
        self.functions.append(func)