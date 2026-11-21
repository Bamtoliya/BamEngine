import ctypes
import struct
import subprocess
import sys
import zlib
from ctypes import wintypes
from pathlib import Path
from tempfile import TemporaryDirectory


def _png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def _build_test_png(width: int = 4, height: int = 4) -> bytes:
    pixels = b"".join(b"\x00" + bytes((255, 0, 0, 255) * width) for _ in range(height))
    data = b"\x89PNG\r\n\x1a\n"
    data += _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += _png_chunk(b"IDAT", zlib.compress(pixels))
    data += _png_chunk(b"IEND", b"")
    return data


def _write_valid_obj(path: Path) -> None:
    path.write_text(
        "o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.2 0.3\nvt 0.4 0.5\nvt 0.6 0.7\nf 1/1 2/2 3/3\n",
        encoding="utf-8",
    )


def _assert_ordered_subsequence(actual: list[str], expected: list[str], stdout: str, stderr: str) -> None:
    cursor = 0
    for line in actual:
        if cursor < len(expected) and line == expected[cursor]:
            cursor += 1
    if cursor != len(expected):
        missing = expected[cursor:]
        raise RuntimeError(f"Missing expected worker responses {missing}:\nstdout:\n{stdout}\nstderr:\n{stderr}")


def _test_corrupt_inputs_and_preexisting_outputs(
    worker: Path, snapshot_check: Path, output_check: Path, root: Path
) -> None:
    valid_obj = root / "valid_triangle.obj"
    _write_valid_obj(valid_obj)

    empty_obj = root / "empty_scene.obj"
    empty_obj.write_text("# empty obj without vertices or faces\n", encoding="utf-8")

    corrupt_gltf = root / "corrupt_mesh.gltf"
    corrupt_gltf.write_bytes(b"{not-valid-json\x00\xff\xfe\xfd")

    valid_png = root / "valid_albedo.png"
    valid_png_bytes = _build_test_png()
    valid_png.write_bytes(valid_png_bytes)

    corrupt_png = root / "corrupt_albedo.png"
    corrupt_png.write_bytes(b"NOT_A_PNG\x00\xff\xfe\xfd")

    truncated_png = root / "truncated_albedo.png"
    truncated_png.write_bytes(valid_png_bytes[: len(valid_png_bytes) // 2])

    out_dir = root / "outputs"
    out_dir.mkdir(exist_ok=True)

    sentinel_bamtex = b"PREEXISTING_BAMTEX_SENTINEL_V1"
    sentinel_dds = b"PREEXISTING_DDS_SENTINEL_V1"
    final_bamtex = out_dir / "existing.bamtex"
    final_dds = out_dir / "existing.dds"
    final_bamtex.write_bytes(sentinel_bamtex)
    final_dds.write_bytes(sentinel_dds)

    rejected_bmdc = out_dir / "rejected.bmdc"
    rejected_bamtex = out_dir / "rejected-stage.bamtex"
    rejected_dds = out_dir / "rejected-stage.dds"
    recovered_bmdc = out_dir / "recovered.bmdc"
    recovered_bamtex = out_dir / "recovered-stage.bamtex"
    recovered_dds = out_dir / "recovered-stage.dds"

    options = "1 0 1 0 0 0 0 1 0.02 0.5 0.25"
    commands = [
        "MODEL_WORKER_VERSION\t1",
        "TEXTURE_WORKER_VERSION\t2",
        "UNKNOWN_CMD\t1\tignored",
        "IMPORT_MODEL\t10\ttruncated_field_count",
        "IMPORT_TEXTURE\t11\ttruncated_field_count",
        f"IMPORT_MODEL\t12\t{valid_obj}\t{rejected_bmdc}\t1 0 1 0 0 0 0 1 nan 0.5 0.25",
        f"IMPORT_MODEL\t13\t{corrupt_gltf}\t{rejected_bmdc}\t{options}",
        f"IMPORT_MODEL\t14\t{empty_obj}\t{rejected_bmdc}\t{options}",
        f"IMPORT_TEXTURE\t15\t{valid_png}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\t999\t11",
        f"IMPORT_TEXTURE\t16\t{valid_png}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\t18\t999",
        f"IMPORT_TEXTURE\t17\t{valid_png}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\tabc\t11",
        f"IMPORT_TEXTURE\t18\t{corrupt_png}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\t18\t11",
        f"IMPORT_TEXTURE\t19\t{truncated_png}\t{final_bamtex}\t{final_dds}\t{rejected_bamtex}\t{rejected_dds}\t18\t11",
        f"IMPORT_MODEL\t20\t{valid_obj}\t{recovered_bmdc}\t{options}",
        f"IMPORT_TEXTURE\t21\t{valid_png}\t{final_bamtex}\t{final_dds}\t{recovered_bamtex}\t{recovered_dds}\t18\t11",
        "QUIT",
    ]

    run = subprocess.run(
        [str(worker), "--daemon", "--project-root", str(root)],
        cwd=root,
        input="\n".join(commands) + "\n",
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=90,
    )
    if run.returncode != 0:
        raise RuntimeError(f"Daemon exited abnormally after bad assets ({run.returncode}):\n{run.stdout}\n{run.stderr}")

    responses = run.stdout.splitlines()
    expected = [
        "MODEL_WORKER_VERSION\t1",
        "TEXTURE_WORKER_VERSION\t2",
        "RESULT\t12\t0",
        "RESULT\t13\t0",
        "RESULT\t14\t0",
        "RESULT\t15\t0",
        "RESULT\t16\t0",
        "RESULT\t17\t0",
        "RESULT\t18\t0",
        "RESULT\t19\t0",
        "PROGRESS\t20\t1",
        "PROGRESS\t20\t2",
        "RESULT\t20\t1",
        "PROGRESS\t21\t1",
        "PROGRESS\t21\t2",
        "PROGRESS\t21\t3",
        "PROGRESS\t21\t4",
        "RESULT\t21\t1",
    ]
    _assert_ordered_subsequence(responses, expected, run.stdout, run.stderr)

    if final_bamtex.read_bytes() != sentinel_bamtex or final_dds.read_bytes() != sentinel_dds:
        raise RuntimeError("Pre-existing final texture outputs were modified during failed or staged requests.")
    if rejected_bmdc.exists() or rejected_bamtex.exists() or rejected_dds.exists():
        raise RuntimeError("Rejected requests left behind unexpected output files.")
    if not recovered_bmdc.is_file() or not recovered_bamtex.is_file() or not recovered_dds.is_file():
        raise RuntimeError("Same-daemon recovery requests did not produce expected outputs.")

    model_check = subprocess.run(
        [str(snapshot_check), str(recovered_bmdc), "--expect-fallback"], cwd=root, capture_output=True, timeout=30
    )
    if model_check.returncode != 0:
        raise RuntimeError(f"Recovered model snapshot invalid:\n{model_check.stderr.decode(errors='replace')}")

    tex_check = subprocess.run(
        [str(output_check), str(recovered_bamtex), str(recovered_dds), "BC7SRGB", "4", "4", "3"],
        cwd=root,
        capture_output=True,
        timeout=15,
    )
    if tex_check.returncode != 0:
        raise RuntimeError(f"Recovered texture outputs invalid:\n{tex_check.stderr.decode(errors='replace')}")


def _test_force_kill_and_restart(worker: Path, snapshot_check: Path, root: Path) -> None:
    proc = subprocess.Popen(
        [str(worker), "--daemon", "--project-root", str(root)],
        cwd=root,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        assert proc.stdin is not None and proc.stdout is not None
        proc.stdin.write(b"MODEL_WORKER_VERSION\t1\n")
        proc.stdin.flush()
        handshake = proc.stdout.readline().decode("utf-8", errors="replace").strip()
        if handshake != "MODEL_WORKER_VERSION\t1":
            raise RuntimeError(f"Unexpected handshake before kill: {handshake!r}")

        proc.kill()
        exit_code = proc.wait(timeout=10)
        if exit_code == 0:
            raise RuntimeError("Force-killed worker unexpectedly returned 0.")
        trailing = proc.stdout.read()
        if trailing != b"":
            raise RuntimeError(f"Expected clean EOF after force-kill, got {len(trailing)} trailing bytes.")
    finally:
        for pipe in (proc.stdin, proc.stdout, proc.stderr):
            if pipe is not None:
                pipe.close()

    recovered_obj = root / "after_kill.obj"
    recovered_bmdc = root / "after_kill.bmdc"
    _write_valid_obj(recovered_obj)
    restart_cmd = "\n".join([
        "MODEL_WORKER_VERSION\t1",
        f"IMPORT_MODEL\t1\t{recovered_obj}\t{recovered_bmdc}\t1 0 1 0 0 0 0 1 0.02 0.5 0.25",
        "QUIT",
        "",
    ])
    run = subprocess.run(
        [str(worker), "--daemon", "--project-root", str(root)],
        cwd=root,
        input=restart_cmd,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=45,
    )
    if run.returncode != 0 or "RESULT\t1\t1" not in run.stdout.splitlines() or not recovered_bmdc.is_file():
        raise RuntimeError(f"Replacement worker failed after kill:\n{run.stdout}\n{run.stderr}")

    model_check = subprocess.run(
        [str(snapshot_check), str(recovered_bmdc), "--expect-fallback"], cwd=root, capture_output=True, timeout=30
    )
    if model_check.returncode != 0:
        raise RuntimeError(f"Replacement worker snapshot invalid:\n{model_check.stderr.decode(errors='replace')}")


def _test_pipe_eof_exit(worker: Path, root: Path) -> None:
    proc = subprocess.Popen(
        [str(worker), "--daemon", "--project-root", str(root)],
        cwd=root,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        assert proc.stdin is not None and proc.stdout is not None
        proc.stdin.write(b"TEXTURE_WORKER_VERSION\t2\n")
        proc.stdin.flush()
        handshake = proc.stdout.readline().decode("utf-8", errors="replace").strip()
        if handshake != "TEXTURE_WORKER_VERSION\t2":
            raise RuntimeError(f"Unexpected handshake before stdin close: {handshake!r}")

        proc.stdin.close()
        exit_code = proc.wait(timeout=10)
        if exit_code != 0:
            raise RuntimeError(f"Worker did not exit cleanly on stdin EOF: exit_code={exit_code}")
    finally:
        for pipe in (proc.stdin, proc.stdout, proc.stderr):
            if pipe is not None and not pipe.closed:
                pipe.close()


def _test_job_object_orphan_cleanup(worker: Path, root: Path) -> None:
    if sys.platform != "win32":
        return

    class IO_COUNTERS(ctypes.Structure):
        _fields_ = [
            ("ReadOperationCount", ctypes.c_uint64),
            ("WriteOperationCount", ctypes.c_uint64),
            ("OtherOperationCount", ctypes.c_uint64),
            ("ReadTransferCount", ctypes.c_uint64),
            ("WriteTransferCount", ctypes.c_uint64),
            ("OtherTransferCount", ctypes.c_uint64),
        ]

    class JOBOBJECT_BASIC_LIMIT_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("PerProcessUserTimeLimit", ctypes.c_int64),
            ("PerJobUserTimeLimit", ctypes.c_int64),
            ("LimitFlags", wintypes.DWORD),
            ("MinimumWorkingSetSize", ctypes.c_size_t),
            ("MaximumWorkingSetSize", ctypes.c_size_t),
            ("ActiveProcessLimit", wintypes.DWORD),
            ("Affinity", ctypes.c_size_t),
            ("PriorityClass", wintypes.DWORD),
            ("SchedulingClass", wintypes.DWORD),
        ]

    class JOBOBJECT_EXTENDED_LIMIT_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("BasicLimitInformation", JOBOBJECT_BASIC_LIMIT_INFORMATION),
            ("IoInfo", IO_COUNTERS),
            ("ProcessMemoryLimit", ctypes.c_size_t),
            ("JobMemoryLimit", ctypes.c_size_t),
            ("PeakProcessMemoryUsed", ctypes.c_size_t),
            ("PeakJobMemoryUsed", ctypes.c_size_t),
        ]

    if ctypes.sizeof(ctypes.c_void_p) == 8 and ctypes.sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION) != 144:
        raise RuntimeError(
            f"Unexpected JOBOBJECT_EXTENDED_LIMIT_INFORMATION size: {ctypes.sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)}"
        )

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.argtypes = [wintypes.LPVOID, wintypes.LPCWSTR]
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID, wintypes.DWORD]
    kernel32.SetInformationJobObject.restype = wintypes.BOOL
    kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS = 9
    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000

    job = kernel32.CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())

    proc = None
    try:
        info = JOBOBJECT_EXTENDED_LIMIT_INFORMATION()
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel32.SetInformationJobObject(
            job, JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS, ctypes.byref(info), ctypes.sizeof(info)
        ):
            raise ctypes.WinError(ctypes.get_last_error())

        proc = subprocess.Popen(
            [str(worker), "--daemon", "--project-root", str(root)],
            cwd=root,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if not kernel32.AssignProcessToJobObject(job, wintypes.HANDLE(int(proc._handle))):
            raise ctypes.WinError(ctypes.get_last_error())

        assert proc.stdin is not None and proc.stdout is not None
        proc.stdin.write(b"MODEL_WORKER_VERSION\t1\n")
        proc.stdin.flush()
        handshake = proc.stdout.readline().decode("utf-8", errors="replace").strip()
        if handshake != "MODEL_WORKER_VERSION\t1":
            raise RuntimeError(f"Unexpected handshake inside job object: {handshake!r}")
        if proc.poll() is not None:
            raise RuntimeError("Worker exited before Job Object handle closure.")

        # Keep stdin open so only JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE can terminate the blocked daemon.
        if not kernel32.CloseHandle(job):
            raise ctypes.WinError(ctypes.get_last_error())
        job = None

        proc.wait(timeout=10)
    finally:
        if job:
            kernel32.CloseHandle(job)
        if proc is not None:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)
            for pipe in (proc.stdin, proc.stdout, proc.stderr):
                if pipe is not None and not pipe.closed:
                    pipe.close()


def main() -> None:
    worker = Path(sys.argv[1]).resolve()
    snapshot_check = Path(sys.argv[2]).resolve()
    output_check = Path(sys.argv[3]).resolve()

    with TemporaryDirectory(prefix="BamAssetWorkerFault-") as directory:
        root = Path(directory)
        _test_corrupt_inputs_and_preexisting_outputs(worker, snapshot_check, output_check, root)
        _test_force_kill_and_restart(worker, snapshot_check, root)
        _test_pipe_eof_exit(worker, root)
        _test_job_object_orphan_cleanup(worker, root)


if __name__ == "__main__":
    main()
