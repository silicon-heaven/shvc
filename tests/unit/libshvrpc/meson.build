unittest_libshvrpc = executable(
  'unittest-libshvrpc',
  [
    'rpcmsg_meta.c',
    'rpcmsg_pack.c',
    'rpcmsg_access.c',
    'rpcurl.c',
    unittest_utils_src,
  ],
  dependencies: [libshvrpc_dep, check_suite, obstack],
  include_directories: [includes, unittest_utils_includes],
)
test(
  'unittest-libshvrpc',
  bash,
  args: [test_driver, unittest_libshvrpc.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvrpc,
)
