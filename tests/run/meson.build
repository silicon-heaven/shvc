python_pytest = python.find_installation(
  'python',
  modules: [
    'pytest',
    'pytest_tap',
    'pytest_asyncio',
    'shv',
  ],
)

pytest_depends = []
pytest_env = []
if shvc_opt
  pytest_depends += shvc
  pytest_env += 'SHVC=' + shvc.full_path()
endif
if shvcp_opt
  pytest_depends += shvcp
  pytest_env += 'SHVCP=' + shvcp.full_path()
endif
if shvcsub_opt
  pytest_depends += shvcsub
  pytest_env += 'SHVCSUB=' + shvcsub.full_path()
endif
if shvctio_opt
  pytest_depends += shvctio
  pytest_env += 'SHVCTIO=' + shvctio.full_path()
endif
if shvcbroker_opt
  pytest_depends += shvcbroker
  pytest_env += 'SHVCBROKER=' + shvcbroker.full_path()
endif
if demo_device_opt
  pytest_depends += demo_device
  pytest_env += 'DEMO_DEVICE=' + demo_device.full_path()
endif
if demo_client_opt
  pytest_depends += demo_client
  pytest_env += 'DEMO_CLIENT=' + demo_client.full_path()
endif

test(
  'run-pytest',
  bash,
  args: [
    test_driver,
    '-n',
    python_pytest.full_path(),
    '-m',
    'pytest',
    '--tap-stream',
    '-vv',
    meson.current_source_dir(),
  ],
  env: pytest_env,
  protocol: 'tap',
  timeout: 240,
  depends: pytest_depends,
)
