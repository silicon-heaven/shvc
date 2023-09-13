unittest_libshvrpc = executable(
  'unittest-libshvrpc',
  [
    'rpcclient_links.c',
    'rpcclient_login.c',
    'rpcclient_ping.c',
    'rpcmsg_access.c',
    'rpcmsg_head.c',
    'rpcmsg_pack.c',
    'rpcurl.c',
    unittest_utils_src,
  ],
  dependencies: [libshvrpc_dep, check_suite, obstack],
  include_directories: [includes, unittest_utils_includes],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvrpc',
  bash,
  args: [test_driver, unittest_libshvrpc.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvrpc,
  timeout: 120,
)
