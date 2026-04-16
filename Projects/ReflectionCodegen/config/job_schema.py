from dataclasses import dataclass


def _normalize_value(value: str) -> str:
    return value.strip().removesuffix("::")


def _normalize_unique(values: tuple[str, ...] | list[str]) -> tuple[str, ...]:
    result: list[str] = []
    seen: set[str] = set()

    for value in values:
        normalized = _normalize_value(value)
        if not normalized or normalized in seen:
            continue
        seen.add(normalized)
        result.append(normalized)

    return tuple(result)


@dataclass(slots=True, frozen=True)
class ReflectionCodegenProfile:
    stripped_namespaces: tuple[str, ...] = ("std",)
    namespace_fallbacks: tuple[str, ...] = ()
    resource_handle_templates: tuple[str, ...] = ()
    ignored_headers: tuple[str, ...] = ("ReflectionMacro.h",)

    def extend(
        self,
        *,
        stripped_namespaces: list[str] | None = None,
        namespace_fallbacks: list[str] | None = None,
        resource_handle_templates: list[str] | None = None,
        ignored_headers: list[str] | None = None,
    ) -> "ReflectionCodegenProfile":
        return ReflectionCodegenProfile(
            stripped_namespaces=_normalize_unique(
                self.stripped_namespaces + tuple(stripped_namespaces or [])
            ),
            namespace_fallbacks=_normalize_unique(
                self.namespace_fallbacks + tuple(namespace_fallbacks or [])
            ),
            resource_handle_templates=_normalize_unique(
                self.resource_handle_templates + tuple(resource_handle_templates or [])
            ),
            ignored_headers=_normalize_unique(
                self.ignored_headers + tuple(ignored_headers or [])
            ),
        )


@dataclass(slots=True, frozen=True)
class ReflectionJob:
    module_name: str
    source_root: str
    output_file: str
    working_directory: str
    profile: ReflectionCodegenProfile


_ACTIVE_PROFILE = ReflectionCodegenProfile()


def set_active_profile(profile: ReflectionCodegenProfile) -> None:
    global _ACTIVE_PROFILE
    _ACTIVE_PROFILE = profile


def get_active_profile() -> ReflectionCodegenProfile:
    return _ACTIVE_PROFILE