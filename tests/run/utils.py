"""Various utilities that are used in multiple tests."""

import subprocess
import asyncio
import collections.abc


def filter_stderr_iter(
    data: collections.abc.Sequence[bytes],
) -> collections.abc.Iterator[bytes]:
    return (line for line in data if not line.startswith(b"=="))


def filter_stderr(data: bytes) -> bytes:
    """Filter out Valgrind lines from stderr."""
    return b"\n".join(filter_stderr_iter(data.split(b"\n")))


async def subproc(
    *cmd: str, exit_code: int = 0, stdin: bytes | None = None
) -> tuple[list[bytes], list[bytes]]:
    proc = await asyncio.create_subprocess_exec(
        cmd[0],
        *cmd[1:],
        stdin=subprocess.PIPE if stdin is not None else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = await proc.communicate(stdin)
    assert proc.returncode is not None
    if proc.returncode != exit_code:
        raise subprocess.CalledProcessError(proc.returncode, cmd, stdout, stderr)
    return stdout.split(b"\n"), list(filter_stderr_iter(stderr.split(b"\n")))
