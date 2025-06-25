import dataclasses
import os
import shlex
import sys

import pytest
import pytest_tap.plugin  # type: ignore
from _pytest.terminal import TerminalReporter  # noqa: PLC2701
from shv.broker import RpcBroker, RpcBrokerConfig
from shv.rpcapi.valueclient import SHVValueClient
from shv.rpcdef import RpcAccess
from shv.rpclogin import RpcLogin, RpcLoginType
from shv.rpcurl import RpcUrl


class MesonTAPPlugin(pytest_tap.plugin.TAPPlugin):
    def pytest_runtest_logreport(self, report: pytest.TestReport):
        super().pytest_runtest_logreport(report)
        if report.failed:
            print(f"{'_' * 16} {report.head_line} {'_' * 16}", file=sys.stderr)
            print(report.longreprtext, file=sys.stderr)
            print(file=sys.stderr)


def pytest_addoption(parser):
    parser.addoption(
        "--meson",
        default=False,
        action="store_true",
    )


@pytest.hookimpl(trylast=True)
def pytest_configure(config):
    if config.getoption("--meson") and not config.option.help:
        config.option.tap_stream = True
        config.option.tap_log_passing_tests = True
        config.pluginmanager.register(MesonTAPPlugin(config), "tapplugin")
        devnull = open("/dev/null", "w", encoding=sys.getdefaultencoding())
        config.pluginmanager.register(
            TerminalReporter(config, devnull), "terminalreporter"
        )


################################################################################


@pytest.fixture(name="valgrind_exec", scope="session")
def fixture_valgrind_exec():
    """Parse and provide valgrind executer and its arugments.

    It is provided through the environment variable.
    """
    env = os.getenv("VALGRIND")
    return shlex.split(env) if env else []


@pytest.fixture(name="shvc_exec", scope="session")
def fixture_shvc_exec(valgrind_exec):
    """Provide the shvc application."""
    env = os.getenv("SHVC")
    if not env:
        pytest.skip("SHVC not provided")
    return [*valgrind_exec, env]


@pytest.fixture(name="shvcs_exec", scope="session")
def fixture_shvcs_exec(valgrind_exec):
    """Provide the shvcs application."""
    env = os.getenv("SHVCS")
    if not env:
        pytest.skip("SHVCS not provided")
    return [*valgrind_exec, env]


@pytest.fixture(name="shvcbroker_exec", scope="session")
def fixture_shvcbroker_exec(valgrind_exec):
    """Provide the shvcbroker application."""
    env = os.getenv("SHVCBROKER")
    if not env:
        pytest.skip("SHVCBROKER not provided")
    return [*valgrind_exec, env]


@pytest.fixture(name="demo_device_exec", scope="session")
def fixture_demo_device_exec(valgrind_exec):
    """Provide the shvc-demo-device application."""
    env = os.getenv("DEMO_DEVICE")
    if not env:
        pytest.skip("DEMO_DEVICE not provided")
    return [*valgrind_exec, env]


@pytest.fixture(name="demo_history_exec", scope="session")
def fixture_demo_history_exec(valgrind_exec):
    """Provide the demo-history application."""
    env = os.getenv("DEMO_HISTORY")
    if not env:
        pytest.skip("DEMO_HISTORY not provided")
    return [*valgrind_exec, env]


@pytest.fixture(name="demo_client_exec", scope="session")
def fixture_demo_client_exec(valgrind_exec):
    """Provide the shvc-demo-client application."""
    env = os.getenv("DEMO_CLIENT")
    if not env:
        pytest.skip("DEMO_CLIENT not provided")
    return [*valgrind_exec, env]


@pytest.fixture(name="port", scope="module")
def fixture_port(unused_tcp_port_factory):
    """TCP/IP port SHV RPC Broker will listen on."""
    return unused_tcp_port_factory()


@pytest.fixture(name="url", scope="module")
def fixture_url(port):
    """RPC URL for TCP/IP communication."""
    return RpcUrl(
        location="localhost",
        port=port,
        login=RpcLogin(
            username="test",
            password="test",
            login_type=RpcLoginType.PLAIN,
        ),
    )


@pytest.fixture(name="admin_url", scope="module")
def fixture_admin_url(url):
    """RPC URL for TCP/IP communication."""
    return dataclasses.replace(
        url,
        login=dataclasses.replace(url.login, username="admin", password="admin!123"),
    )


@pytest.fixture(name="broker_config", scope="module")
def fixture_broker_config(url):
    """Provide configuration for SHV RPC Broker."""
    config = RpcBrokerConfig()
    config.listen = [url]
    config.roles.add(
        config.Role(
            "tester",
            {"test/*"},
            {
                RpcAccess.BROWSE: {"**:ls", "**:dir"},
                RpcAccess.COMMAND: {"test/**:*"},
                RpcAccess.SERVICE: {"test/.history/**:*"},
            },
        )
    )
    config.users.add(config.User("test", "test", {"tester"}))
    return config


@pytest.fixture(name="broker")
async def fixture_broker(broker_config):
    """SHV RPC Broker implemented in Python."""
    broker = RpcBroker(broker_config)
    await broker.start_serving()
    yield broker
    await broker.terminate()


@pytest.fixture(name="client")
async def fixture_client(broker, url):
    """SHV RPC Client connected to the Python Broker."""
    client = await SHVValueClient.connect(url)
    yield client
    await client.disconnect()
