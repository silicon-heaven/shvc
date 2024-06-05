unittest_libshvhistory = executable(
  'unittest-libshvhistory',
  [
    'rpchistory.c',
    unittest_utils_src,
  ],
  dependencies: [libshvhistory_dep, check_suite, obstack],
  include_directories: [includes, unittest_utils_includes],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvhistory',
  bash,
  args: [test_driver, unittest_libshvhistory.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvhistory,
  timeout: 120,
)
