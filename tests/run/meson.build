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
  pytest_env += 'SHVC=' + shvc.path()
endif
if shvcbroker_opt
  pytest_depends += shvcbroker
  pytest_env += 'SHVCBROKER=' + shvcbroker.path()
endif
if demo_device_opt
  pytest_depends += demo_device
  pytest_env += 'DEMO_DEVICE=' + demo_device.path()
endif
if demo_client_opt
  pytest_depends += demo_client
  pytest_env += 'DEMO_CLIENT=' + demo_client.path()
endif

test(
  'run-pytest',
  bash,
  args: [
    test_driver,
    '-n',
    python_pytest.path(),
    '-m',
    'pytest',
    '--tap-stream',
    meson.current_source_dir(),
  ],
  env: pytest_env,
  protocol: 'tap',
  timeout: 240,
  depends: pytest_depends,
)
