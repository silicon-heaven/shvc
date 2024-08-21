"""Check the shvcbroker implementation."""

import socket
import dataclasses
import logging
import contextlib
import asyncio
import shv
import pytest
from .device import Device

logger = logging.getLogger(__name__)


@pytest.fixture(name="shvcbroker")
async def fixture_shvcbroker(shvcbroker_exec, tmp_path, url):
    """SHVC broker."""
    conf = tmp_path / "config.cpon"
    with conf.open("w") as file:
        file.write(
            shv.Cpon.pack(
                {
                    "name": "testbroker",
                    "listen": [str(url)],
                    "users": {
                        "admin": {"password": "admin!123", "role": "admin"},
                        "test": {"password": "test", "role": "test"},
                    },
                    "roles": {
                        "admin": {"mountPoints": "**", "access": {"dev": "**:*"}},
                        "test": {
                            "mountPoints": "test/**",
                            "access": {"cmd": "test/**:*", "bws": ["**:ls", "**:dir"]},
                        },
                    },
                }
            )
        )

    cmd = [shvcbroker_exec[0], *shvcbroker_exec[1:], "-d", "-c", conf]
    proc = await asyncio.create_subprocess_exec(*cmd)

    while True:
        with contextlib.suppress(OSError):
            with socket.create_connection((url.location, url.port)):
                break
    yield proc
    proc.terminate()
    await proc.wait()
    assert proc.returncode == 0


@pytest.fixture(name="client")
async def fixture_client(shvcbroker, url):
    """SHV RPC Client connected to the Python Broker."""
    client = await shv.ValueClient.connect(url)
    yield client
    await client.disconnect()


@pytest.fixture(name="admin_client")
async def fixture_admin_client(shvcbroker, admin_url):
    """SHV RPC Client with admin rights connected to the Python Broker."""
    client = await shv.ValueClient.connect(admin_url)
    yield client
    await client.disconnect()


@pytest.fixture(name="device")
async def fixture_device(shvcbroker, url):
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


@pytest.mark.parametrize(
    "path,result",
    (
        ("", [".app", ".broker", "test"]),
        (".app", []),
        (".broker", ["currentClient", "client"]),
        (".broker/client", ["1", "2"]),
        (".broker/client/1", [".app"]),
        (".broker/client/1/.app", []),
        (".broker/client/2", [".app", "value", "lock"]),
        (".broker/currentClient", []),
        ("test", ["device"]),
        ("test/device", [".app", "value", "lock"]),
    ),
)
async def test_ls(client, device, path, result):
    """Check ls."""
    assert await client.ls(path) == result


@pytest.mark.parametrize(
    "path,result",
    (
        (
            ".broker",
            [
                shv.RpcMethodDesc.stddir(),
                shv.RpcMethodDesc.stdls(),
                shv.RpcMethodDesc.getter(
                    name="name",
                    param="Null",
                    result="String",
                    access=shv.RpcMethodAccess.BROWSE,
                ),
                shv.RpcMethodDesc(
                    name="clientInfo",
                    param="Int",
                    result="ClientInfo",
                    access=shv.RpcMethodAccess.SUPER_SERVICE,
                ),
                shv.RpcMethodDesc(
                    name="mountedClientInfo",
                    param="String",
                    result="ClientInfo",
                    access=shv.RpcMethodAccess.SUPER_SERVICE,
                ),
                shv.RpcMethodDesc(
                    name="clients",
                    param="Null",
                    result="List[Int]",
                    access=shv.RpcMethodAccess.SUPER_SERVICE,
                ),
                shv.RpcMethodDesc(
                    name="mounts",
                    param="Null",
                    result="List[String]",
                    access=shv.RpcMethodAccess.SUPER_SERVICE,
                ),
                shv.RpcMethodDesc(
                    name="disconnectClient",
                    param="Int",
                    result="Null",
                    access=shv.RpcMethodAccess.SUPER_SERVICE,
                ),
            ],
        ),
        (
            ".broker/client",
            [
                shv.RpcMethodDesc.stddir(),
                shv.RpcMethodDesc.stdls(),
            ],
        ),
        (
            ".broker/currentClient",
            [
                shv.RpcMethodDesc.stddir(),
                shv.RpcMethodDesc.stdls(),
                shv.RpcMethodDesc.getter(
                    name="info",
                    param="Null",
                    result="ClientInfo",
                    access=shv.RpcMethodAccess.BROWSE,
                ),
                shv.RpcMethodDesc(
                    name="subscribe",
                    param="String",
                    result="Bool",
                    access=shv.RpcMethodAccess.BROWSE,
                ),
                shv.RpcMethodDesc(
                    name="unsubscribe",
                    param="String",
                    result="Bool",
                    access=shv.RpcMethodAccess.BROWSE,
                ),
                shv.RpcMethodDesc(
                    name="subscriptions",
                    param="Null",
                    result="List[String]",
                    access=shv.RpcMethodAccess.BROWSE,
                ),
            ],
        ),
    ),
)
async def test_dir(client, path, result):
    """Check dir."""
    assert await client.dir(path) == result


@pytest.mark.parametrize(
    "path,method,result",
    (
        (".broker", "clients", True),
        (".broker", "invalid", False),
        (".broker/currentClient", "info", True),
        (".broker/currentClient", "invalid", False),
    ),
)
async def test_dir_exists(client, path, method, result):
    """Check dir."""
    assert await client.dir_exists(path, method) == result


@pytest.mark.parametrize(
    "path,method,param,result",
    (
        (".broker", "name", None, "testbroker"),
        (
            ".broker/currentClient",
            "info",
            None,
            {"clientId": 1, "role": "test", "subscriptions": {}, "userName": "test"},
        ),
        (".broker/currentClient", "subscriptions", None, {}),
        ("test/device/value", "get", None, 42),
    ),
)
async def test_call(client, device, path, method, param, result):
    """Call various methods from Python."""
    assert await client.call(path, method, param) == result


@pytest.mark.parametrize(
    "path,method,param,result",
    (
        (".broker/client/2/value", "get", None, 42),
        (
            ".broker",
            "clientInfo",
            1,
            {"clientId": 1, "role": "admin", "subscriptions": {}, "userName": "admin"},
        ),
        (
            ".broker",
            "clientInfo",
            2,
            {
                "clientId": 2,
                "mountPoint": "test/device",
                "role": "test",
                "subscriptions": {},
                "userName": "test",
            },
        ),
        (".broker", "clientInfo", 42, None),
        (
            ".broker",
            "mountedClientInfo",
            "test/device",
            {
                "clientId": 2,
                "mountPoint": "test/device",
                "role": "test",
                "subscriptions": {},
                "userName": "test",
            },
        ),
        (
            ".broker",
            "mountedClientInfo",
            "test/device/value",
            {
                "clientId": 2,
                "mountPoint": "test/device",
                "role": "test",
                "subscriptions": {},
                "userName": "test",
            },
        ),
        (".broker", "mountedClientInfo", "test", None),
        (".broker", "clients", None, [1, 2]),
        (".broker", "mounts", None, ["test/device"]),
        (
            ".broker/currentClient",
            "info",
            None,
            {"clientId": 1, "role": "admin", "subscriptions": {}, "userName": "admin"},
        ),
    ),
)
async def test_admin_call(admin_client, device, path, method, param, result):
    """Call various methods from Python with admin access level."""
    assert await admin_client.call(path, method, param) == result


@pytest.mark.parametrize(
    "path,method",
    (
        (".broker", "clients"),
        (".broker", "clientInfo"),
        (".broker", "mounts"),
        (".broker", "mountedClientInfo"),
    ),
)
async def test_not_enough_access(client, path, method):
    """Call various methods that exist but test user doesn't have access."""
    with pytest.raises(shv.RpcMethodNotFoundError):
        assert await client.call(path, method)


async def test_invalid_login(shvcbroker, url):
    nurl = dataclasses.replace(
        url, login=dataclasses.replace(url.login, password="invalid")
    )
    with pytest.raises(shv.RpcMethodCallExceptionError, match=r"Invalid login"):
        await shv.SimpleClient.connect(nurl)


@pytest.mark.parametrize(
    "path",
    (
        "",
        ".app",
        ".app/mount",
        ".broker",
        ".broker/mount",
    ),
)
async def test_invalid_mount(shvcbroker, admin_url, path):
    nurl = dataclasses.replace(
        admin_url,
        login=dataclasses.replace(
            admin_url.login, options={"device": {"mountPoint": path}}
        ),
    )
    with pytest.raises(
        shv.RpcMethodCallExceptionError, match=r"Mount point not allowed"
    ):
        await shv.SimpleClient.connect(nurl)


async def test_not_allowed_mount(shvcbroker, url):
    nurl = dataclasses.replace(
        url,
        login=dataclasses.replace(
            url.login, options={"device": {"mountPoint": "toplevel/device"}}
        ),
    )
    with pytest.raises(
        shv.RpcMethodCallExceptionError, match=r"Mount point not allowed"
    ):
        await shv.SimpleClient.connect(nurl)


@pytest.mark.parametrize(
    "path",
    (
        "test/device",
        "test/device/foo",
        "test",
    ),
)
async def test_mount_exists(shvcbroker, device, admin_url, path):
    nurl = dataclasses.replace(
        admin_url,
        login=dataclasses.replace(
            admin_url.login, options={"device": {"mountPoint": path}}
        ),
    )
    with pytest.raises(
        shv.RpcMethodCallExceptionError, match=r"Mount point already exists"
    ):
        await shv.SimpleClient.connect(nurl)


async def test_client_id(client, device):
    assert await client.call("test/device/lock", "owner") is None
    assert await client.call("test/device/lock", "take") is None
    assert await client.call("test/device/lock", "owner") == "test:testbroker"


async def test_subscribe(client):
    sub = "test/device/*:*:*"
    assert await client.subscribe(sub) is True
    assert await client.subscribe(sub) is False
    assert await client.call(".broker/currentClient", "subscriptions") == {sub: None}
    assert await client.unsubscribe(sub) is True
    assert await client.call(".broker/currentClient", "subscriptions") == {}
    assert await client.unsubscribe(sub) is False


async def test_unsub_disconnect(client):
    """Check that subscription is correctly freed on client disconnect."""
    sub = "test/device/*:*:*"
    assert await client.subscribe(sub) is True


async def test_unsub_reset(client):
    """Check that subscription is correctly removed on client reset."""
    sub = "test/device/*:*:*"
    # client.subscribe can't be used because reset would perform resubscribe
    assert await client.call(".broker/currentClient", "subscribe", sub) is True
    assert await client.call(".broker/currentClient", "subscriptions") == {sub: None}
    await client.reset()
    assert await client.call(".broker/currentClient", "subscriptions") == {}


async def test_subscribe_ttl(client):
    sub = "test/device/*:*:*"
    assert await client.call(".broker/currentClient", "subscribe", [sub, 30]) is True
    subs = await client.call(".broker/currentClient", "subscriptions")
    assert sub in subs
    assert isinstance(subs[sub], int)
    assert await client.call(".broker/currentClient", "subscribe", [sub, 130]) is False
    subs = await client.call(".broker/currentClient", "subscriptions")
    assert subs[sub] > 100
