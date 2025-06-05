unittest_libshvcp = executable(
  'unittest-libshvcp',
  [
    'chainpack.c',
    'chainpackh.c',
    'cph.c',
    'cp_pack.c',
    'cp_unpack.c',
    'cpdatetime.c',
    'cpdecimal.c',
    'cpitem.c',
    'cpon.c',
    unittest_utils_src,
  ],
  dependencies: [libshvrpc_dep, check_suite, obstack],
  include_directories: [includes, unittest_utils_includes],
  c_args: unittest_utils_c_args,
)
test(
  'unittest-libshvcp',
  unittest_libshvcp,
  env: unittests_env,
  protocol: 'tap',
  timeout: 120,
  suite: ['unit', 'libshvcp'],
)
