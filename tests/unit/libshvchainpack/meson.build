unittest_libshvchainpack = executable(
  'unittest-libshvchainpack',
  [
    'packunpack.c',
    'chainpack.c',
    'cpon.c',
  ],
  dependencies: [libshvrpc_dep, check_suite, obstack],
  include_directories: includes,
)
test(
  'unittest-libshvchainpack',
  bash,
  args: [test_driver, unittest_libshvchainpack.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvchainpack,
)
