"""Check if our demo applications communicate with each other."""

import asyncio
import logging
import shv

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
        "-d",
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
                {1: "dir", 3: "idir", 4: "odir", 5: 1},
                {1: "ls", 3: "ils", 4: "ols", 5: 1, 6: {"lsmod": "olsmod"}},
            ],
        ),
        ("test/device", "ls", [".app", "track"]),
        (
            "test/device/.app",
            "dir",
            [
                {1: "dir", 3: "idir", 4: "odir", 5: 1},
                {1: "ls", 3: "ils", 4: "ols", 5: 1, 6: {"lsmod": "olsmod"}},
                {1: "shvVersionMajor", 2: 2, 4: "Int", 5: 1},
                {1: "shvVersionMinor", 2: 2, 4: "Int", 5: 1},
                {1: "name", 2: 2, 4: "String", 5: 1},
                {1: "version", 2: 2, 4: "String", 5: 1},
                {1: "ping", 5: 1},
                {1: "date", 2: 2, 4: "DateTime", 5: 1},
            ],
        ),
        ("test/device/.app", "ls", []),
        ("test/device/.app", "shvVersionMajor", 3),
        ("test/device/.app", "shvVersionMinor", 0),
        ("test/device/.app", "name", "shvc-demo-device"),
        ("test/device/.app", "version", version),
        ("test/device/.app", "ping", None),
        (
            "test/device/track",
            "dir",
            [
                {1: "dir", 3: "idir", 4: "odir", 5: 1},
                {1: "ls", 3: "ils", 4: "ols", 5: 1, 6: {"lsmod": "olsmod"}},
            ],
        ),
        ("test/device/track", "ls", [str(i) for i in range(1, 10)]),
        *((f"test/device/track/{i}", "ls", []) for i in range(1, 10)),
        *(
            (
                f"test/device/track/{i}",
                "dir",
                [
                    {1: "dir", 3: "idir", 4: "odir", 5: 1},
                    {1: "ls", 3: "ils", 4: "ols", 5: 1, 6: {"lsmod": "olsmod"}},
                    {1: "get", 2: 2, 4: "[Int]", 5: 8, 6: {"chng": None}},
                    {1: "set", 2: 4, 3: "[Int]", 5: 16},
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


@pytest.mark.parametrize(
    "path,method,param,error",
    (
        ("test/device/track/none", "ls", None, shv.RpcMethodNotFoundError),
        ("test/device/track/none", "dir", None, shv.RpcMethodNotFoundError),
        ("test/device/track/none", "set", None, shv.RpcMethodNotFoundError),
        ("test/device/track/1", "set", None, shv.RpcInvalidParamsError),
    ),
)
async def test_call_error(demo_device, client, path, method, param, error):
    """Call various methods from Python."""
    with pytest.raises(error):
        assert await client.call(path, method, param)


@pytest.mark.parametrize("i", range(1, 10))
async def test_set(demo_device, client, i):
    """Test that track nodes can be modified."""
    signal = asyncio.Future()

    def callback(c, pth, param):
        signal.set_result([pth, param])

    path = f"test/device/track/{i}"
    await client.prop_get(path) == list(range(1, i))
    client.on_change(path, callback)
    await client.subscribe(shv.RpcSubscription(paths=path))
    await client.prop_set(path, [32, 42, 52])
    logger.error("Before signal for change")
    assert await signal == [path, [32, 42, 52]]
    logger.error("Waiting for change")
    client.on_change(path, None)
    logger.error("Change done")
    await client.prop_get(path) == [32, 42, 52]


async def test_demo_client(demo_device, demo_client_exec, url):
    stdout, stderr = await subproc(*demo_client_exec, str(url))
    assert stdout == [
        b"The '.app:name' is: pyshvbroker",
        b"Demo device's track 4: 1 2 3 4",
        b"New demo device's track 4: 2 3 4 5",
        b"",
    ]
    assert stderr == [b""]


async def test_demo_client_without_device(broker, demo_client_exec, url):
    stdout, stderr = await subproc(*demo_client_exec, str(url), exit_code=1)
    assert stdout == [
        b"The '.app:name' is: pyshvbroker",
        b"",
    ]
    assert stderr == [
        b"Error: Device not mounted at 'test/device'.",
        b"Hint: Make sure the device is connected to the broker.",
        b"",
    ]
