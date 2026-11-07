from dataclasses import dataclass, field


@dataclass(slots=True)
class ReflectedParameter:
    cpp_type: str
    name: str

    def normalized_type(self) -> str:
        return " ".join(self.cpp_type.strip().split())


@dataclass(slots=True)
class FunctionQualifiers:
    is_const: bool = False
    is_volatile: bool = False
    ref_qualifier: str = ""
    is_noexcept: bool = False

    def to_suffix_tokens(self) -> list[str]:
        tokens: list[str] = []

        if self.is_const:
            tokens.append("const")
        if self.is_volatile:
            tokens.append("volatile")
        if self.ref_qualifier:
            tokens.append(self.ref_qualifier)
        if self.is_noexcept:
            tokens.append("noexcept")

        return tokens

    def to_suffix_string(self) -> str:
        tokens = self.to_suffix_tokens()
        if not tokens:
            return ""
        return " " + " ".join(tokens)


@dataclass(slots=True)
class ReflectedFunction:
    name: str
    signature: str
    owner_name: str
    owner_qualified_name: str
    return_type_raw: str
    params: list[ReflectedParameter] = field(default_factory=list)
    qualifiers: FunctionQualifiers = field(default_factory=FunctionQualifiers)

    def normalized_return_type(self) -> str:
        return " ".join(self.return_type_raw.strip().split())

    def has_params(self) -> bool:
        return len(self.params) > 0

    def make_signature_text(self) -> str:
        if not self.params:
            return f"{self.name}()"

        param_types = ", ".join(param.normalized_type() for param in self.params)
        return f"{self.name}({param_types})"

    def overload_suffix(self) -> str:
        if not self.params:
            return "void"

        parts: list[str] = []
        for param in self.params:
            clean = param.normalized_type()
            clean = clean.replace("const ", "")
            clean = clean.replace("&", "")
            clean = clean.replace("*", "Ptr")
            clean = clean.replace("::", "_")

            sanitized = []
            for char in clean:
                if char.isalnum() or char == "_":
                    sanitized.append(char)
                else:
                    sanitized.append("_")

            token = "".join(sanitized).strip("_")
            parts.append(token or "Unknown")

        return "_".join(parts) if parts else "void"