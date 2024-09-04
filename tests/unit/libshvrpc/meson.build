unittest_libshvrpc = executable(
  'unittest-libshvrpc',
  [
    'crc32.c',
    'rpcaccess.c',
    'rpcclient.c',
    'rpcclient_stream.c',
    'rpcdir.c',
    'rpcerror.c',
    'rpclogin.c',
    'rpcmsg_head.c',
    'rpcmsg_pack.c',
    'rpcri.c',
    'rpctransport.c',
    'rpcurl.c',
    unittest_utils_src,
  ],
  dependencies: [libshvrpc_dep, check_suite],
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
  suite: ['libshvrpc'],
)

unittest_libshvrpc_internal = executable(
  'unittest-libshvrpc-internal',
  [
    'strset.c',
    libshvrpc_sources,
    unittest_utils_src,
  ],
  dependencies: [libshvrpc_dep, check_suite],
  include_directories: [
    includes,
    unittest_utils_includes,
    libshvrpc_internal_includes,
  ],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvrpc-internal',
  bash,
  args: [test_driver, unittest_libshvrpc_internal.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libshvrpc_internal,
  suite: ['libshvrpc-internal'],
)
