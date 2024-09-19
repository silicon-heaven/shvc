"""Help texts and other CLI interface for applications."""

from .__version__ import version
from .utils import subproc


async def test_shvc_help(shvc_exec):
    stdout, stderr = await subproc(*shvc_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{shvc_exec[-1]} [-ivqdVh] [-u URL] [-t SEC] [PATH:METHOD] [PARAM]".encode(),
        b"SHV RPC client call.",
        b"",
        b"Arguments:",
        b"  -u URL   Where to connect to (default tcp://test@localhost?password=test)",
        b"  -t SEC   Number of seconds before call is abandoned (default 300)",
        b"  -i       Read parameter from STDIN instead of argument",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximal logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_shvcsub_help(shvcs_exec):
    stdout, stderr = await subproc(*shvcs_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{shvcs_exec[-1]} [-vqdVh] [-u URL] [RI]...".encode(),
        b"SHV RPC subscription client.",
        b"",
        b"Arguments:",
        b"  -u URL   Where to connect to (default tcp://test@localhost?password=test)",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximal logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_shvcbroker_help(shvcbroker_exec):
    stdout, stderr = await subproc(*shvcbroker_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{shvcbroker_exec[-1]} [-vqdVh] [-c FILE]".encode(),
        b"SHV RPC Broker.",
        b"",
        b"Arguments:",
        b"  -c FILE  Path to the configuration file",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximal logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_demo_device_help(demo_device_exec):
    stdout, stderr = await subproc(*demo_device_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{demo_device_exec[-1]} [-v] [URL]".encode(),
        b"Example SHV RPC device implemented using SHVC.",
        b"",
        b"Arguments:",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximum logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_demo_history_help(demo_history_exec):
    stdout, stderr = await subproc(*demo_history_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{demo_history_exec[-1]} [-v] [URL]".encode(),
        b"Example SHV RPC history implemented using SHVC.",
        b"",
        b"Arguments:",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximum logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_demo_client_help(demo_client_exec):
    stdout, stderr = await subproc(*demo_client_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{demo_client_exec[-1]} [-v] [URL]".encode(),
        b"Example SHV RPC client implemented using SHVC.",
        b"",
        b"Arguments:",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximum logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_shvc_version(shvc_exec):
    stdout, stderr = await subproc(*shvc_exec, "-V")
    assert stdout == [f"{shvc_exec[-1]} {version}".encode(), b""]
    assert stderr == [b""]


async def test_shvcbroker_version(shvcbroker_exec):
    stdout, stderr = await subproc(*shvcbroker_exec, "-V")
    assert stdout == [f"{shvcbroker_exec[-1]} {version}".encode(), b""]
    assert stderr == [b""]


async def test_demo_device_version(demo_device_exec):
    stdout, stderr = await subproc(*demo_device_exec, "-V")
    assert stdout == [f"{demo_device_exec[-1]} {version}".encode(), b""]
    assert stderr == [b""]


async def test_demo_history_version(demo_history_exec):
    stdout, stderr = await subproc(*demo_history_exec, "-V")
    assert stdout == [f"{demo_history_exec[-1]} {version}".encode(), b""]
    assert stderr == [b""]


async def test_demo_client_version(demo_client_exec):
    stdout, stderr = await subproc(*demo_client_exec, "-V")
    assert stdout == [f"{demo_client_exec[-1]} {version}".encode(), b""]
    assert stderr == [b""]
