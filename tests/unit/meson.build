check_suite = dependency('check-suite')
obstack = (
  cc.has_function('obstack_free') ? declare_dependency() : cc.find_library('obstack')
)

unittests_env = [
  'CK_TAP_LOG_FILE_NAME=/dev/stdout',
  'CK_VERBOSITY=silent',
]

unittest_libfoo = executable(
  'unittest-libfoo',
  [
    'count.c',
  ],
  dependencies: [libfoo_dep, check_suite, obstack],
  include_directories: includes,
)
test(
  'unittest-libfoo',
  unittest_libfoo,
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_libfoo,
)

unittest_foo = executable(
  'unittest-foo',
  foo_sources + ['config.c'],
  dependencies: foo_dependencies + [check_suite, obstack],
  include_directories: [includes, foo_internal_includes],
)
test(
  'unittest-foo',
  unittest_foo,
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_foo,
  suite: ['unit'],
)
