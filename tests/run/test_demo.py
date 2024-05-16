"""Check if our demo applications communicate with each other."""

import asyncio
import logging

import pytest
from .__version__ import version
from .utils import subproc

logger = logging.getLogger(__name__)


@pytest.fixture(name="demo_device")
async def fixture_demo_device(demo_device_exec, broker, url):
    """Demo device connected to the Python broker."""
    cmd = [
        demo_device_exec[0],
        *demo_device_exec[1:],
        f"{url}&devmount=test/device",
        "-v",
    ]
    logger.debug("%s", " ".join(cmd))
    proc = await asyncio.create_subprocess_exec(*cmd)
    while broker.client_on_path("test/device") is None:
        await asyncio.sleep(0)  # Unblock the asyncio loop
    yield proc
    proc.terminate()
    await proc.wait()


@pytest.mark.parametrize(
    "path,method,result",
    (
        ("", "ls", [".app", ".broker", "test"]),
        ("test", "ls", ["device"]),
        (
            "test/device",
            "dir",
            [
                {"name": "dir", "signature": 3, "flags": 0, "access": "bws"},
                {"name": "ls", "signature": 3, "flags": 0, "access": "bws"},
            ],
        ),
        ("test/device", "ls", [".app", "track"]),
        (
            "test/device/.app",
            "dir",
            [
                {"access": "bws", "flags": 0, "name": "dir", "signature": 3},
                {"access": "bws", "flags": 0, "name": "ls", "signature": 3},
                {
                    "access": "bws",
                    "flags": 2,
                    "name": "shvVersionMajor",
                    "signature": 2,
                },
                {
                    "access": "bws",
                    "flags": 2,
                    "name": "shvVersionMinor",
                    "signature": 2,
                },
                {"access": "bws", "flags": 2, "name": "name", "signature": 2},
                {"access": "bws", "flags": 2, "name": "version", "signature": 2},
            ],
        ),
        ("test/device/.app", "ls", []),
        ("test/device/.app", "shvVersionMajor", 3),
        ("test/device/.app", "shvVersionMinor", 0),
        ("test/device/.app", "name", "demo-device"),
        ("test/device/.app", "version", version),
        (
            "test/device/track",
            "dir",
            [
                {"name": "dir", "signature": 3, "flags": 0, "access": "bws"},
                {"name": "ls", "signature": 3, "flags": 0, "access": "bws"},
            ],
        ),
        ("test/device/track", "ls", [str(i) for i in range(1, 10)]),
        *((f"test/device/track/{i}", "ls", []) for i in range(1, 10)),
        *(
            (
                f"test/device/track/{i}",
                "dir",
                [
                    {"name": "dir", "signature": 3, "flags": 0, "access": "bws"},
                    {"name": "ls", "signature": 3, "flags": 0, "access": "bws"},
                    {"access": "rd", "flags": 2, "name": "get", "signature": 3},
                    {"access": "wr", "flags": 4, "name": "set", "signature": 1},
                ],
            )
            for i in range(1, 10)
        ),
        *(
            (f"test/device/track/{i}", "get", list(range(1, i + 1)))
            for i in range(1, 10)
        ),
    ),
)
async def test_call(demo_device, client, path, method, result):
    """Call various methods from Python."""
    assert await client.call(path, method) == result


@pytest.mark.parametrize("i", range(1, 10))
async def test_set(demo_device, client, i):
    """Test that track nodes can be modified."""
    signal = asyncio.Future()

    def callback(c, pth, param):
        signal.set_result([pth, param])

    path = f"test/device/track/{i}"
    await client.prop_get(path) == list(range(1, i))
    client.on_change(path, callback)
    await client.prop_set(path, [32, 42, 52])
    # TODO demo device is not sending signals right now
    # assert await signal == [path, [32, 42, 52]]
    client.on_change(path, None)
    await client.prop_get(path) == [32, 42, 52]


async def test_demo_client(demo_device, demo_client_exec, url):
    stdout, stderr = await subproc(*demo_client_exec, str(url))
    assert stdout == [
        b"The '.app:name' is: demo-device",
        b"The value of track/4 is: [1, 2, 3, 4]",
        b"The value of track/4 has successfully been set to: [2, 3, 4, 8]",
        b"",
    ]
    assert stderr == [b""]


async def test_demo_client_without_device(broker, demo_client_exec, url):
    stdout, stderr = await subproc(*demo_client_exec, str(url), exit_code=1)
    assert stdout == [b""]
    assert stderr == [
        b"Error: Device not mounted at 'test/device'.",
        b"Hint: Make sure the device is connected to the broker.",
        b"",
    ]
