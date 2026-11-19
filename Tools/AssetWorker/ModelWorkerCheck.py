import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


def main():
    worker = Path(sys.argv[1]).resolve()
    with TemporaryDirectory(prefix="BamAssetWorkerModel-") as directory:
        root = Path(directory)
        source = root / "한글 모델.obj"
        snapshot = root / "변환 결과.bmdc"
        rejected = root / "missing.bmdc"
        source.write_text("o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", encoding="utf-8")
        options = "1 0 1 0 0 0 0 1 0.02 0.5 0.25"
        commands = [
            "MODEL_WORKER_VERSION\t1",
            f"IMPORT_MODEL\t1\t{source}\t{snapshot}\t{options}",
            f"IMPORT_MODEL\t2\t{root / 'missing.obj'}\t{rejected}\t{options}",
            "QUIT",
        ]
        run = subprocess.run(
            [str(worker), "--daemon", "--project-root", str(root)],
            cwd=root,
            input="\n".join(commands) + "\n",
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )
        responses = run.stdout.splitlines()
        required = ["MODEL_WORKER_VERSION\t1", "PROGRESS\t1\t1", "PROGRESS\t1\t2", "RESULT\t1\t1", "RESULT\t2\t0"]
        actual = [line for line in responses if line in required]
        if run.returncode != 0 or actual != required:
            raise RuntimeError(f"Unexpected worker response: {run.returncode}\n{run.stdout}\n{run.stderr}")
        if not snapshot.is_file() or snapshot.stat().st_size <= 8 or rejected.exists():
            raise RuntimeError("Worker success must produce a snapshot and failure must produce no snapshot.")
        with snapshot.open("rb") as stream:
            if stream.read(4) != b"BMDC":
                raise RuntimeError("Worker snapshot header is missing.")


if __name__ == "__main__":
    main()
