import re


BEGIN_NAMESPACE_PATTERN = re.compile(r'BEGIN\s*\(\s*([A-Za-z_]\w*(?:::\w+)*)\s*\)')
CPP_NAMESPACE_PATTERN = re.compile(r'namespace\s+([A-Za-z_]\w*(?:::\w+)*)\s*\{')
END_NAMESPACE_PATTERN = re.compile(r'(?m)^\s*END\s*$')


def split_namespace(ns: str) -> list[str]:
    if not ns:
        return []
    return [part.strip() for part in ns.split("::") if part.strip()]


def make_qualified_name(namespace_parts: list[str], local_name: str) -> str:
    if not namespace_parts:
        return local_name
    return "::".join(namespace_parts + [local_name])


class NamespaceTracker:
    def __init__(self, content: str):
        self.content = content
        self.events = self._build_events(content)

    def _build_events(self, content: str) -> list[tuple[int, str, list[str]]]:
        events: list[tuple[int, str, list[str]]] = []

        for match in BEGIN_NAMESPACE_PATTERN.finditer(content):
            namespace_parts = split_namespace(match.group(1))
            events.append((match.start(), "push", namespace_parts))

        for match in CPP_NAMESPACE_PATTERN.finditer(content):
            namespace_parts = split_namespace(match.group(1))
            events.append((match.start(), "push", namespace_parts))

        for match in END_NAMESPACE_PATTERN.finditer(content):
            events.append((match.start(), "pop", []))

        events.sort(key=lambda item: item[0])
        return events

    def get_namespace_at(self, offset: int) -> list[str]:
        stack: list[str] = []

        for position, kind, namespace_parts in self.events:
            if position > offset:
                break

            if kind == "push":
                stack.extend(namespace_parts)
            elif kind == "pop":
                if stack:
                    stack.pop()

        return stack.copy()