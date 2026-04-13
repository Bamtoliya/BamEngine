from dataclasses import dataclass, field


@dataclass(slots=True)
class ResolvedTypeRef:
    raw_name: str
    resolved_name: str = ""
    resolved_qualified_name: str = ""
    property_kind: str = ""


@dataclass
class ContainerTypeNode:
    container_kind: str
    raw_type: str
    emitted_type_name: str = ""
    key_type: ResolvedTypeRef | None = None
    value_type: ResolvedTypeRef | None = None
    inner_container: "ContainerTypeNode | None" = None


@dataclass(slots=True)
class ReflectedProperty:
    attributes_raw: str
    cpp_type: str
    name: str

    resolved_type: ResolvedTypeRef | None = None
    container_type: ContainerTypeNode | None = None

    def normalized_cpp_type(self) -> str:
        return " ".join(self.cpp_type.strip().split())

    def has_attributes(self) -> bool:
        return bool(self.attributes_raw and self.attributes_raw.strip())

    def is_container(self) -> bool:
        return self.container_type is not None