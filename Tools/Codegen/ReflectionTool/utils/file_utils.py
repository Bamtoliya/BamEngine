import os


def read_file_content(filepath: str) -> str:
    for encoding in ("utf-8", "cp949"):
        try:
            with open(filepath, "r", encoding=encoding) as file:
                return file.read()
        except Exception:
            continue

    return ""


def write_text_file(filepath: str, content: str, encoding: str = "utf-8") -> None:
    os.makedirs(os.path.dirname(filepath), exist_ok=True)

    with open(filepath, "w", encoding=encoding) as file:
        file.write(content)