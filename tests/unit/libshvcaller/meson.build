unittest_libshvcaller = executable(
  'unittest-libshvcaller',
  [
    unittest_utils_src,
  ],
  dependencies: [libshvcaller_dep, check_suite],
  include_directories: [includes, unittest_utils_includes],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvcaller',
  bash,
  args: [test_driver, unittest_libshvcaller.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvcaller,
  timeout: 120,
)
