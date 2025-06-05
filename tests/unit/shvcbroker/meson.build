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
  unittest_shvcbroker,
  env: unittests_env,
  protocol: 'tap',
  timeout: 120,
  suite: ['unit', 'shvcbroker'],
)
