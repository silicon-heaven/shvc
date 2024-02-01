import os
import shlex

import pytest


@pytest.fixture(name="valgrind_exec", scope="session")
def fixture_valgrind_exec():
    """Parse and provide valgrind executer as passed by environment variable."""
    env = os.getenv("VALGRIND")
    yield shlex.split(env) if env else []
