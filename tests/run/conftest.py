import os
import shlex
import shv
import shv.broker

import pytest


@pytest.fixture(name="valgrind_exec", scope="session")
def fixture_valgrind_exec():
    """Parse and provide valgrind executer and its arugments

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
    return shv.RpcUrl(
        location="localhost",
        port=port,
        login=shv.RpcLogin(
            username="test",
            password="test",
            login_type=shv.RpcLoginType.PLAIN,
        ),
    )


@pytest.fixture(name="broker_config", scope="module")
def fixture_broker_config(url):
    """Configuration for SHV RPC Broker."""
    config = shv.broker.RpcBrokerConfig()
    config.listen = [url]
    config.roles.add(
        config.Role(
            "tester",
            {"test/*"},
            {
                shv.RpcMethodAccess.BROWSE: {"**:ls", "**:dir"},
                shv.RpcMethodAccess.COMMAND: {"test/**:*"},
            },
        )
    )
    config.users.add(config.User("test", "test", {"tester"}))
    return config


@pytest.fixture(name="broker")
async def fixture_broker(broker_config):
    """SHV RPC Broker implemented in Python."""
    broker = shv.broker.RpcBroker(broker_config)
    await broker.start_serving()
    yield broker
    await broker.terminate()


@pytest.fixture(name="client")
async def fixture_client(broker, url):
    """SHV RPC Client connected to the Python Broker."""
    client = await shv.ValueClient.connect(url)
    yield client
    await client.disconnect()
