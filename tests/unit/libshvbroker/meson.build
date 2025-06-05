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
  unittest_libshvbroker,
  env: unittests_env,
  protocol: 'tap',
  suite: ['unit', 'libshvbroker'],
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
  unittest_libshvbroker_internal,
  env: unittests_env,
  protocol: 'tap',
  suite: ['unit', 'libshvbroker'],
)
