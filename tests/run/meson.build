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
  'run',
  python,
  args: [
    '-m', 'pytest',
    '--tap-stream',
    '--log-level=DEBUG',
    '-vv', meson.current_source_dir(),
  ],
  env: ['NO_VALGRIND_WRAP=y'] + pytest_env,
  depends: pytest_depends,
  timeout: 1000,
  protocol: 'tap',
  suite: ['run'],
)
