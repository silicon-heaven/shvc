python_pytest = python.find_installation(
  'python',
  modules: [
    'pytest',
    'pytest_tap',
    'shv',
  ],
)

pytest_run = false
pytest_args = [
  test_driver,
  '-n',
  python_pytest.path(),
  '-m',
  'pytest',
  '--tap-stream',
]
pytest_depends = []
# TODO demo device and demo client and broker and so on
pytest_args += [meson.current_source_dir()]

if pytest_run
  test(
    'run-pytest',
    bash,
    args: pytest_args,
    protocol: 'tap',
    timeout: 240,
    depends: pytest_depends,
  )
endif
