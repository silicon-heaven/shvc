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
if not shvc_opt.disabled()
  pytest_depends += shvc
  pytest_env += 'SHVC=' + shvc.full_path()
endif
if not shvcs_opt.disabled()
  pytest_depends += shvcs
  pytest_env += 'SHVCS=' + shvcs.full_path()
endif
if not shvcp_opt.disabled()
  pytest_depends += shvcp
  pytest_env += 'SHVCP=' + shvcp.full_path()
endif
if not shvctio_opt.disabled()
  pytest_depends += shvctio
  pytest_env += 'SHVCTIO=' + shvctio.full_path()
endif
if not shvcbroker_opt.disabled()
  pytest_depends += shvcbroker
  pytest_env += 'SHVCBROKER=' + shvcbroker.full_path()
endif
if not demo_device_opt.disabled()
  pytest_depends += demo_device
  pytest_env += 'DEMO_DEVICE=' + demo_device.full_path()
endif
if not demo_client_opt.disabled()
  pytest_depends += demo_client
  pytest_env += 'DEMO_CLIENT=' + demo_client.full_path()
endif
if not demo_history_opt.disabled()
  pytest_depends += demo_history
  pytest_env += 'DEMO_HISTORY=' + demo_history.full_path()
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
    '--log-level=DEBUG',
    '-vv',
    meson.current_source_dir(),
  ],
  env: pytest_env,
  protocol: 'tap',
  timeout: 1000,
  depends: pytest_depends,
  suite: ['run'],
)
