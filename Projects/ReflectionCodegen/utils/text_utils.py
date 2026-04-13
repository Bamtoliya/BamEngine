import re


def normalize_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text.strip())


def mask_comments(content: str) -> str:
    content = re.sub(
        r"/\*.*?\*/",
        lambda match: " " * len(match.group(0)),
        content,
        flags=re.DOTALL,
    )
    content = re.sub(
        r"//.*",
        lambda match: " " * len(match.group(0)),
        content,
    )
    return content


def mask_string_literals(content: str) -> str:
    pattern = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
    return pattern.sub(lambda match: " " * len(match.group(0)), content)


def mask_comments_and_strings(content: str) -> str:
    without_comments = mask_comments(content)
    return mask_string_literals(without_comments)