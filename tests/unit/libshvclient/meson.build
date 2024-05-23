unittest_libshvclient = executable(
  'unittest-libshvclient',
  [
    unittest_utils_src,
  ],
  dependencies: [libshvclient_dep, check_suite],
  include_directories: [includes, unittest_utils_includes],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvclient',
  bash,
  args: [test_driver, unittest_libshvclient.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvclient,
  timeout: 120,
)
