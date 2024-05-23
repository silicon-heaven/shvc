"""Check the functionality of shvc."""

import dataclasses
import pytest
from .utils import subproc


@pytest.mark.parametrize(
    "path,method,result",
    (
        ("", "ls", '[".app",".broker"]'),
        (".app", "ls", "[]"),
        (".app", "name", '"pyshvbroker"'),
        (".app", "ping", "null"),
    ),
)
async def test_valid(shvc_exec, url, broker, path, method, result):
    stdout, stderr = await subproc(*shvc_exec, "-u", str(url), path, method)
    assert stdout == [result.encode(), b""]
    assert stderr == [b""]


async def test_param(shvc_exec, url, broker):
    """Check that we can pass parameter."""
    stdout, stderr = await subproc(
        *shvc_exec, "-d", "-u", str(url), ".broker/currentClient", "subscribe", "{}"
    )
    assert stdout == [b"true", b""]
    assert stderr[0] == b'=> <1:1,8:1,10:"hello">i{1:null}'
    # Note we skip the nonce message line because that is not predictable
    assert stderr[2:] == [
        b'=> <1:1,8:2,10:"login">i{1:{"login":{"password":"test","user":"test","ty'
        b'pe":"PLAIN"}}}',
        b"<= <8:2>i{2...",
        b'=> <1:1,8:4,9:".broker/currentClient",10:"subscribe">i{1:{}}',
        b"<= <8:4>i{2:true...",
        b"",
    ]


async def test_param_stdin(shvc_exec, url, broker):
    """Check that we can pass parameter over stdin."""
    stdout, stderr = await subproc(
        *shvc_exec,
        "-i",
        "-u",
        str(url),
        ".broker/currentClient",
        "subscribe",
        stdin=b"{}",
    )
    assert stdout == [b"true", b""]
    assert stderr == [b""]


@pytest.mark.parametrize(
    "path,method,error,exit_code",
    (
        ("test", "ls", 'No such node: test', 2),
        (
            ".app",
            "invalid",
            "No such path '.app' or method 'invalid' or access rights.",
            2,
        ),
    ),
)
async def test_invalid(shvc_exec, url, broker, path, method, error, exit_code):
    stdout, stderr = await subproc(
        *shvc_exec, "-u", str(url), path, method, exit_code=exit_code
    )
    assert stdout == [b""]
    assert stderr == [f"SHV Error: {error}".encode(), b""]


async def test_invalid_url(shvc_exec):
    stdout, stderr = await subproc(*shvc_exec, "-u", "invalid:/dev/null", exit_code=3)
    assert stdout == [b""]
    assert stderr == [
        b"Invalid URL: invalid:/dev/null",
        b"             ^",
        b"",
    ]


async def test_not_running_broker(shvc_exec, url):
    stdout, stderr = await subproc(*shvc_exec, "-u", str(url), exit_code=3)
    assert stdout == [b""]
    assert stderr == [
        f"Failed to connect to the: {url}".encode(),
        b"Please check your connection to the network",
        b"",
    ]


async def test_invalid_login(shvc_exec, url, broker):
    nurl = dataclasses.replace(
        url, login=dataclasses.replace(url.login, password="invalid")
    )
    stdout, stderr = await subproc(*shvc_exec, "-u", str(nurl), exit_code=3)
    assert stdout == [b""]
    assert stderr == [
        f"Invalid login for connecting to the: {nurl}".encode(),
        b"Invalid login",
        b"",
    ]
