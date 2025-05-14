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
  bash,
  args: [test_driver, unittest_libfoo.full_path()],
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
  bash,
  args: [test_driver, unittest_foo.full_path()],
  env: unittests_env,
  protocol: 'tap',
  depends: unittest_foo,
  suite: ['unit'],
)
