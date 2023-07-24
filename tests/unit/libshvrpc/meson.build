unittest_libshvrpc = executable(
  'unittest-libshvrpc',
  [
    'rpcurl.c',
  ],
  dependencies: [libshvrpc_dep, check_suite, obstack],
  include_directories: includes,
)
test(
  'unittest-libshvrpc',
  bash,
  args: [test_driver, unittest_libshvrpc.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvrpc,
)
