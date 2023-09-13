"""Check that cpconv works correctly."""
import subprocess

import pytest


@pytest.fixture(name="cpconv", scope="function")
def fixture_shvcall(request, valgrind_exec):
    def cpconv(*args: str, check: bool = True) -> subprocess.CompletedProcess:
        """Run cpconv with given arguments."""
        res = subprocess.run(
            (*valgrind_exec, request.config.cpconv, *args),
            stdout=subprocess.PIPE,
            check=check,
        )
        return res

    if request.config.cpconv is None:
        pytest.skip("cpconv not provided")
    yield cpconv


def test_help(cpconv):
    """Just simple check that help works."""
    cpconv("-h")

def test_version(cpconv):
    """Just simple check that version works."""
    cpconv("-V")

def test_both_op(cpconv):
    """Usage of both -p and -u is not allowed."""
    assert cpconv("-p", "-u", check=False).returncode == 2

def test_invalid_opt(cpconv):
    """Just some invalid option to cover that code."""
    assert cpconv("-x", check=False).returncode == 2

def test_invalid_arg(cpconv):
    """Just some invalid argument to cover that code."""
    assert cpconv("input", "output", "invalid", check=False).returncode == 2
