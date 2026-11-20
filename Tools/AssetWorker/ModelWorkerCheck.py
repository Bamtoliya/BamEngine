import subprocess
import sys
import base64
import json
import struct
from pathlib import Path
from tempfile import TemporaryDirectory


def main():
    worker = Path(sys.argv[1]).resolve()
    snapshot_check = Path(sys.argv[2]).resolve()
    with TemporaryDirectory(prefix="BamAssetWorkerModel-") as directory:
        root = Path(directory)
        source = root / "한글 모델.obj"
        snapshot = root / "변환 결과.bmdc"
        rejected = root / "missing.bmdc"
        source.write_text("o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.2 0.3\nvt 0.4 0.5\nvt 0.6 0.7\nf 1/1 2/2 3/3\n", encoding="utf-8")
        gltf_source = root / "two_uvs.gltf"
        gltf_snapshot = root / "two_uvs.bmdc"
        payload = struct.pack("<9f12f3H", 0, 0, 0, 1, 0, 0, 0, 1, 0,
                              0, 0, 1, 0, 0, 1, 0.25, 0.25, 0.75, 0.25, 0.25, 0.75, 0, 1, 2)
        gltf = {
            "asset": {"version": "2.0"},
            "buffers": [{"uri": "data:application/octet-stream;base64," + base64.b64encode(payload).decode("ascii"),
                         "byteLength": len(payload)}],
            "bufferViews": [
                {"buffer": 0, "byteOffset": 0, "byteLength": 36},
                {"buffer": 0, "byteOffset": 36, "byteLength": 24},
                {"buffer": 0, "byteOffset": 60, "byteLength": 24},
                {"buffer": 0, "byteOffset": 84, "byteLength": 6},
            ],
            "accessors": [
                {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]},
                {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
                {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"},
                {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
            ],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1, "TEXCOORD_1": 2}, "indices": 3}]}],
            "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
        }
        gltf_source.write_text(json.dumps(gltf), encoding="utf-8")
        options = "1 0 1 0 0 0 0 1 0.02 0.5 0.25"
        commands = [
            "MODEL_WORKER_VERSION\t1",
            f"IMPORT_MODEL\t1\t{source}\t{snapshot}\t{options}",
            f"IMPORT_MODEL\t2\t{root / 'missing.obj'}\t{rejected}\t{options}",
            f"IMPORT_MODEL\t3\t{gltf_source}\t{gltf_snapshot}\t{options}",
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
        required = ["MODEL_WORKER_VERSION\t1", "PROGRESS\t1\t1", "PROGRESS\t1\t2", "RESULT\t1\t1", "RESULT\t2\t0", "RESULT\t3\t1"]
        actual = [line for line in responses if line in required]
        if run.returncode != 0 or actual != required:
            raise RuntimeError(f"Unexpected worker response: {run.returncode}\n{run.stdout}\n{run.stderr}")
        if not snapshot.is_file() or snapshot.stat().st_size <= 8 or rejected.exists():
            raise RuntimeError("Worker success must produce a snapshot and failure must produce no snapshot.")
        with snapshot.open("rb") as stream:
            if stream.read(4) != b"BMDC":
                raise RuntimeError("Worker snapshot header is missing.")

        for path, mode in [(snapshot, "--expect-fallback"), (gltf_snapshot, "--expect-distinct")]:
            validation = subprocess.run(
                [str(snapshot_check), str(path), mode], cwd=root, capture_output=True, timeout=30,
            )
            if validation.returncode != 0:
                raise RuntimeError(
                    f"Worker snapshot could not be consumed: {validation.returncode}\n"
                    f"{validation.stdout.decode(errors='replace')}\n"
                    f"{validation.stderr.decode(errors='replace')}"
                )


if __name__ == "__main__":
    main()
