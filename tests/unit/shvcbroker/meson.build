unittest_shvcbroker = executable(
  'unittest-shvcbroker',
  [
    'config.c',
    unittest_utils_src,
    shvcbroker_sources,
  ],
  dependencies: shvcbroker_dependencies + [check_suite, obstack],
  include_directories: [
    includes,
    shvcbroker_internal_includes,
    unittest_utils_includes,
  ],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-shvcbroker',
  bash,
  args: [test_driver, unittest_shvcbroker.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_shvcbroker,
  timeout: 120,
  suite: ['shvcbroker'],
)
