"""Check that we can communicate over all different transport layers.

This is communication between pyshv and shvc.
"""

from __future__ import annotations

import asyncio
import collections.abc
import contextlib
import dataclasses
import io
import logging
import os
import pty
import select
import threading

import pytest
from shv.broker import RpcBroker, RpcBrokerConfig
from shv.rpcurl import RpcProtocol

from .utils import subproc

logger = logging.getLogger(__name__)


@pytest.fixture(name="lurl", scope="module")
def fixture_lurl(url):
    return dataclasses.replace(
        url,
        login=dataclasses.replace(url.login, options={"device": {"deviceId": "foo"}}),
    )


class RPCTransport:
    @pytest.fixture(name="broker")
    async def fixture_broker(self, urls):
        broker = RpcBroker(
            RpcBrokerConfig(
                listen=[urls[0]],
                roles=[RpcBrokerConfig.Role("tester")],
                users=[RpcBrokerConfig.User("test", "test", {"tester"})],
                autosetups=[
                    RpcBrokerConfig.Autosetup(
                        {"foo"},
                        {"tester"},
                        None,
                        {"test/**:*:*chng", "**:*:chng", "test/**:ls:lsmod"},
                    )
                ],
            )
        )
        await broker.start_serving()
        yield broker
        with contextlib.suppress(asyncio.CancelledError):
            await broker.terminate()

    async def test_ping(self, shvc_exec, broker, urls):
        """Check that shvc can ping python broker over various transport layers."""
        stdout, stderr = await subproc(*shvc_exec, "-u", str(urls[1]))
        assert stdout == [b"null", b""]
        assert stderr == [b""]

    async def test_ls(self, shvc_exec, broker, urls):
        """Check that shvc can ping python broker over various transport layers."""
        stdout, stderr = await subproc(
            *shvc_exec, "-u", str(urls[1]), ".broker/currentClient:subscriptions"
        )
        assert stdout == [
            b'{"**:*:chng":null,"test/**:ls:lsmod":null,"test/**:*:*chng":null}',
            b"",
        ]
        assert stderr == [b""]


class TestTCP(RPCTransport):
    @pytest.fixture(name="urls")
    def fixture_urls(self, lurl):
        yield lurl, lurl


class TestTCPS(RPCTransport):
    @pytest.fixture(name="urls")
    def fixture_urls(self, lurl):
        nurl = dataclasses.replace(lurl, protocol=RpcProtocol.TCPS)
        return nurl, nurl


class TestUnix(RPCTransport):
    @pytest.fixture(name="urls")
    def fixture_urls(self, lurl, tmp_path):
        nurl = dataclasses.replace(
            lurl, location=str(tmp_path / "socket"), protocol=RpcProtocol.UNIX
        )
        return nurl, nurl


class TestUnixS(RPCTransport):
    @pytest.fixture(name="urls")
    def fixture_urls(self, lurl, tmp_path):
        nurl = dataclasses.replace(
            lurl, location=str(tmp_path / "socket"), protocol=RpcProtocol.UNIXS
        )
        return nurl, nurl


class TestCAN(RPCTransport):
    @pytest.fixture(name="urls")
    def fixture_urls(self, lurl):
        iface = os.getenv("SHVC_TEST_CANIF")
        if iface is None:
            pytest.skip("Missing SHVC_TEST_CANIF environment variable")
        # TODO remove can_address when dynamic address is supported
        nurl = dataclasses.replace(
            lurl, location=iface, port=84, protocol=RpcProtocol.CAN, can_address=120
        )
        return nurl, nurl


@dataclasses.dataclass
class PTYPort:
    """PTY based exchange port."""

    port1: str
    port2: str
    event: threading.Event

    @classmethod
    @contextlib.contextmanager
    def new(cls) -> collections.abc.Iterator[PTYPort]:
        """Context function that provides PTY based ports.

        :return: Tuple with two strings containing paths to the pty ports and
          event that can be cleared to pause data exchange and set to resume it
          again.
        """
        pty1_master, pty1_slave = pty.openpty()
        pty2_master, pty2_slave = pty.openpty()
        event = threading.Event()
        event.set()
        thread = threading.Thread(
            target=cls._exchange, args=(pty1_master, pty2_master, event)
        )
        thread.start()
        yield cls(os.ttyname(pty1_slave), os.ttyname(pty2_slave), event)
        os.close(pty1_slave)
        os.close(pty2_slave)
        thread.join()

    @staticmethod
    def _exchange(fd1: int, fd2: int, event: threading.Event) -> None:
        p = select.poll()
        p.register(fd1, select.POLLIN | select.POLLPRI)
        p.register(fd2, select.POLLIN | select.POLLPRI)
        fds = 2
        while fds:
            for fd, pevent in p.poll():
                event.wait()
                if pevent & select.POLLHUP:
                    p.unregister(fd)
                    fds -= 1
                if pevent & select.POLLIN:
                    data = os.read(fd, io.DEFAULT_BUFFER_SIZE)
                    os.write(fd1 if fd is fd2 else fd2, data)
                if pevent & select.POLLERR:
                    logger.error("Error detected in ptycopy")
                    return


class TestTTY(RPCTransport):
    @pytest.fixture(name="urls")
    def fixture_urls(self, lurl):
        with PTYPort.new() as pty:
            yield (
                dataclasses.replace(lurl, location=pty.port1, protocol=RpcProtocol.TTY),
                dataclasses.replace(lurl, location=pty.port2, protocol=RpcProtocol.TTY),
            )
