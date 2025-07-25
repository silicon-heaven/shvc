"""Check if our demo applications communicate with each other."""

import asyncio
import dataclasses
import datetime
import logging

import pytest
from shv.rpcdef import RpcInvalidParamError, RpcMethodNotFoundError

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
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
            ],
        ),
        ("test/device", "ls", [".app", "file", "track"]),
        (
            "test/device/.app",
            "dir",
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "shvVersionMajor", 2: 2, 4: "i", 5: 1},
                {1: "shvVersionMinor", 2: 2, 4: "i", 5: 1},
                {1: "name", 2: 2, 4: "s", 5: 1},
                {1: "version", 2: 2, 4: "s", 5: 1},
                {1: "ping", 5: 1},
                {1: "date", 2: 2, 4: "t", 5: 1},
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
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
            ],
        ),
        ("test/device/track", "ls", [str(i) for i in range(1, 10)]),
        *((f"test/device/track/{i}", "ls", []) for i in range(1, 10)),
        *(
            (
                f"test/device/track/{i}",
                "dir",
                [
                    {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                    {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                    {1: "get", 2: 2, 3: "i(0,)|n", 4: "[i]", 5: 8, 6: {"chng": None}},
                    {1: "set", 2: 4, 3: "[i]", 5: 16},
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
        ("test/device/track/none", "ls", None, RpcMethodNotFoundError),
        ("test/device/track/none", "dir", None, RpcMethodNotFoundError),
        ("test/device/track/none", "set", None, RpcMethodNotFoundError),
        ("test/device/track/1", "set", None, RpcInvalidParamError),
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
    assert await client.prop_get(path) == list(range(1, i + 1))
    client.on_change(path, callback)
    await client.subscribe(f"{path}:*:*")
    await client.prop_set(path, [32, 42, 52])
    assert await signal == [path, [32, 42, 52]]
    client.on_change(path, None)
    assert await client.prop_get(path) == [32, 42, 52]


async def test_device_invalid_login(demo_device_exec, url, broker):
    nurl = dataclasses.replace(
        url, login=dataclasses.replace(url.login, password="invalid")
    )
    stdout, stderr = await subproc(*demo_device_exec, str(nurl), exit_code=1)
    assert stdout == [b""]
    assert stderr == [
        b"Login failure: Invalid login",
        b"",
    ]


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


@pytest.fixture(name="demo_history")
async def fixture_demo_history(demo_history_exec, broker, url):
    """Demo history connected to the Python broker."""
    cmd = [
        demo_history_exec[0],
        *demo_history_exec[1:],
        f"{url}&devmount=test/.history",
        "-d",
    ]
    logger.debug("%s", " ".join(cmd))
    proc = await asyncio.create_subprocess_exec(*cmd)
    while broker.client_on_path("test/.history") is None:
        await asyncio.sleep(0)  # Unblock the asyncio loop
    yield proc
    proc.terminate()
    await proc.wait()


@pytest.mark.parametrize(
    "path,method,param,result",
    (
        ("", "ls", None, [".app", ".broker", "test"]),
        ("test", "ls", None, [".history"]),
        ("test/.history", "ls", None, [".app", ".records", "node0", "node1", "node2"]),
        ("test/.history/node0", "ls", None, ["subnode"]),
        ("test/.history/node1", "ls", None, ["subnode"]),
        ("test/.history/node2", "ls", None, []),
        ("test/.history/node0/subnode", "ls", None, ["1", "2"]),
        (
            "test/.history/node0",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node1",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node2",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node0/subnode",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node0/subnode/1",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node0/subnode/2",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node1/subnode",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node2",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {1: "getLog", 2: 8, 3: "!getLogP", 4: "!getLogR", 5: 1},
                {1: "getSnapshot", 2: 8, 3: "!getSnapshotP", 4: "!getLogR", 5: 1},
            ],
        ),
        (
            "test/.history/node0/subnode",
            "getLog",
            {
                1: datetime.datetime(1970, 1, 1, tzinfo=datetime.UTC),
                2: datetime.datetime(1970, 1, 2, tzinfo=datetime.UTC),
                3: 5,
                4: "**:*",
            },
            [
                {
                    1: datetime.datetime(1970, 1, 1, 0, 2, 30, tzinfo=datetime.UTC),
                    3: "node0/subnode/2",
                    6: None,
                },
                {
                    1: datetime.datetime(1970, 1, 1, 0, 13, 20, tzinfo=datetime.UTC),
                    3: "node0/subnode/1",
                    6: None,
                    7: "elluser_local",
                },
            ],
        ),
        (
            "test/.history/node0/subnode",
            "getSnapshot",
            {
                1: datetime.datetime(1970, 1, 2, tzinfo=datetime.UTC),
                2: "**:*",
            },
            [
                {
                    1: datetime.datetime(1970, 1, 1, 0, 2, 30, tzinfo=datetime.UTC),
                    3: "node0/subnode/2",
                    6: None,
                },
                {
                    1: datetime.datetime(1970, 1, 1, 0, 13, 20, tzinfo=datetime.UTC),
                    3: "node0/subnode/1",
                    6: None,
                    7: "elluser_local",
                },
            ],
        ),
        (
            "test/.history/.records",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
            ],
        ),
        ("test/.history/.records", "ls", None, ["records_log"]),
        (
            "test/.history/.records/records_log",
            "dir",
            None,
            [
                {1: "dir", 3: "n|b|s", 4: "[!dir]|b", 5: 1},
                {1: "ls", 3: "s|n", 4: "[s]|b", 5: 1, 6: {"lsmod": "{b}"}},
                {
                    1: "fetch",
                    2: 8,
                    3: "[i:offset,i(0,):count]",
                    4: "!historyRecords",
                    5: 40,
                },
                {1: "span", 2: 2, 4: "[i:smallest,i:biggest,i(1,):span]", 5: 40},
            ],
        ),
        ("test/.history/.records/records_log", "span", None, [1, 6, 6]),
        (
            "test/.history/.records/records_log",
            "fetch",
            [1, 6],
            [
                {
                    0: 1,
                    1: datetime.datetime(1970, 1, 1, 0, 2, 30, tzinfo=datetime.UTC),
                    2: "node0/subnode/2",
                    5: 2,
                    9: 2,
                },
                {
                    0: 1,
                    1: datetime.datetime(1970, 1, 1, 0, 8, 20, tzinfo=datetime.UTC),
                    2: "node2",
                    3: "fchng",
                    4: "src",
                    5: 3,
                    6: 1,
                    7: "elluser",
                    9: 3,
                },
                {
                    0: 3,
                    1: datetime.datetime(1970, 1, 1, 0, 8, 20, tzinfo=datetime.UTC),
                    9: 4,
                    60: 18000000,
                },
                {
                    0: 1,
                    1: datetime.datetime(1970, 1, 1, 0, 10, tzinfo=datetime.UTC),
                    2: "node1/subnode",
                    5: 5,
                    6: 16,
                    7: "elluser_wifi",
                    9: 5,
                },
                {
                    0: 1,
                    1: datetime.datetime(1970, 1, 1, 0, 13, 20, tzinfo=datetime.UTC),
                    2: "node0/subnode/1",
                    5: 6,
                    6: 40,
                    7: "elluser_local",
                    9: 6,
                },
            ],
        ),
    ),
)
async def test_call_history(demo_history, client, path, method, param, result):
    """Call various methods from Python."""
    assert await client.call(path, method, param) == result


@pytest.mark.parametrize(
    "path,method,param,error",
    (
        ("test/.history/node0/subnode1", "ls", None, RpcMethodNotFoundError),
        ("test/.history/node0/subnode1", "dir", None, RpcMethodNotFoundError),
        ("test/.history/node0/subnode1", "getLog", None, RpcMethodNotFoundError),
        ("test/.history/node/subnode1", "getLog", None, RpcMethodNotFoundError),
        ("test/.history/node0/subnode", "getLog", None, RpcInvalidParamError),
        ("test/.history/.records", "getLog", None, RpcMethodNotFoundError),
        ("test/.history/.records/records_log", "fetch", None, RpcInvalidParamError),
        ("test/.history/.records/records_log", "fetch", ["a", 1], RpcInvalidParamError),
        ("test/.history/.records/records_log", "fetch", [1, "a"], RpcInvalidParamError),
    ),
)
async def test_call_error_history(demo_history, client, path, method, param, error):
    """Call various methods from Python."""
    with pytest.raises(error):
        assert await client.call(path, method, param)
