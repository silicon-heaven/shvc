import os
import shlex
import subprocess
import sys
import time

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--cpconv",
        help="Path to cpconv device binary",
        metavar="PATH",
        nargs=1,
    )


def pytest_configure(config):
    cpconv = config.getoption("--cpconv")
    setattr(config, "cpconv", cpconv[0] if cpconv else None)


@pytest.fixture(name="valgrind_exec", scope="session")
def fixture_valgrind_exec():
    """Parse and provide valgrind executer as passed by environment variable."""
    env = os.getenv("VALGRIND")
    yield shlex.split(env) if env else []
