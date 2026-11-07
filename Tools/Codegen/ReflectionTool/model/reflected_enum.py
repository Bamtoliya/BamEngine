from dataclasses import dataclass, field


@dataclass(slots=True)
class ReflectedEnum:
    name: str
    qualified_name: str
    namespace_parts: list[str]
    entries: list[str] = field(default_factory=list)
    source_file: str = ""

    @property
    def namespace(self) -> str:
        return "::".join(self.namespace_parts)

    @property
    def short_name(self) -> str:
        return self.name

    def is_scoped(self) -> bool:
        return "::" in self.qualified_name