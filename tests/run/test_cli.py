"""Help texts and other CLI interface for applications."""

from .__version__ import version

from .utils import subproc


async def test_shvc_help(shvc_exec):
    stdout, stderr = await subproc(*shvc_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{shvc_exec[-1]} [-ujvqdVh] [PATH] [METHOD] [PARAM]".encode(),
        b"SHV RPC client.",
        b"",
        b"Arguments:",
        b"  -u URL   Where to connect to (default tcp://test@localhost?password=test)",
        b"  -j       Output in JSON format instead of CPON",
        b"  -v       Increase logging level of the communication",
        b"  -q       Decrease logging level of the communication",
        b"  -d       Set maximul logging level of the communication",
        b"  -V       Print SHVC version and exit",
        b"  -h       Print this help text",
        b"",
    ]


async def test_shvcbroker_help(shvcbroker_exec):
    stdout, stderr = await subproc(*shvcbroker_exec, "-h")
    assert stdout == [b""]
    assert stderr == [
        f"{shvcbroker_exec[-1]} [-Vh] [-c FILE]".encode(),
        b"",
        b"Arguments:",
        b"  -c FILE  Configuration file",
        b"  -V       Print version and exit",
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


async def test_demo_client_version(demo_client_exec):
    stdout, stderr = await subproc(*demo_client_exec, "-V")
    assert stdout == [f"{demo_client_exec[-1]} {version}".encode(), b""]
    assert stderr == [b""]
