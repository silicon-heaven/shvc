"""Check functionality of shvcs."""

import dataclasses
import asyncio
import asyncio.subprocess
import pytest
from .device import Device
from .utils import subproc


@pytest.fixture(name="device")
async def fixture_device(broker, url):
    """Device connected to the broker."""
    nurl = dataclasses.replace(
        url,
        login=dataclasses.replace(
            url.login, options={"device": {"mountPoint": "test/device"}}
        ),
    )
    dev = await Device.connect(nurl)
    yield dev
    await dev.disconnect()


async def test_valid(shvcs_exec, url, broker, device):
    proc = await asyncio.create_subprocess_exec(
        *shvcs_exec, "-du", str(url), stdout=asyncio.subprocess.PIPE
    )
    while "**:*:*" not in {sub[0] for sub in broker.subscriptions()}:
        await asyncio.sleep(0)
    try:
        await device.call("test/device/value", "set", 24)
        assert await proc.stdout.readline() == b"<test/device/value:get:chng>24\n"
        await device.call("test/device/value", "set", "foo")
        assert await proc.stdout.readline() == b'<test/device/value:get:chng>"foo"\n'
    finally:
        proc.terminate()
        assert await proc.wait() == 0


async def test_invalid_url(shvcs_exec):
    stdout, stderr = await subproc(*shvcs_exec, "-u", "invalid:/dev/null", exit_code=3)
    assert stdout == [b""]
    assert stderr == [
        b"Invalid URL: invalid:/dev/null",
        b"             ^",
        b"",
    ]


async def test_connect_failed(shvcs_exec, url):
    stdout, stderr = await subproc(*shvcs_exec, "-u", str(url), exit_code=3)
    assert stdout == [b""]
    assert stderr == [
        f"Failed to connect to the: {url}".encode(),
        b"Please check your connection to the destination.",
        b"",
    ]


async def test_invalid_login(shvcs_exec, url, broker):
    nurl = dataclasses.replace(
        url, login=dataclasses.replace(url.login, password="invalid")
    )
    stdout, stderr = await subproc(*shvcs_exec, "-u", str(nurl), exit_code=1)
    assert stdout == [b""]
    assert stderr == [
        b"Login failure: Invalid login",
        b"",
    ]
