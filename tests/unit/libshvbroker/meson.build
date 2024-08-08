unittest_libshvbroker = executable(
  'unittest-libshvbroker',
  [
    unittest_utils_src,
  ],
  dependencies: [libshvbroker_dep, check_suite],
  include_directories: [includes, unittest_utils_includes],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvbroker',
  bash,
  args: [test_driver, unittest_libshvbroker.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvbroker,
  suite: ['libshvbroker'],
)

unittest_libshvbroker_internal = executable(
  'unittest-libshvbroker-internal',
  [
    'nbool.c',
    libshvbroker_sources,
    unittest_utils_src,
  ],
  dependencies: [libshvbroker_dep, check_suite],
  include_directories: [
    includes,
    unittest_utils_includes,
    libshvbroker_internal_includes,
  ],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvbroker-internal',
  bash,
  args: [test_driver, unittest_libshvbroker_internal.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvbroker_internal,
  suite: ['libshvbroker-internal'],
)
