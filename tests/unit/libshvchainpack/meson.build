unittest_libshvchainpack = executable(
  'unittest-libshvchainpack',
  [
    'chainpack.c',
    'chainpackh.c',
    'cp_pack.c',
    'cp_unpack.c',
    'cpdatetime.c',
    'cpdecimal.c',
    'cpon.c',
    unittest_utils_src,
  ],
  dependencies: [libshvrpc_dep, check_suite, obstack],
  include_directories: [includes, unittest_utils_includes],
)
test(
  'unittest-libshvchainpack',
  bash,
  args: [test_driver, unittest_libshvchainpack.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvchainpack,
  timeout: 120,
)
