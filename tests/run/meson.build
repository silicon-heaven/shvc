bats = find_program('bats')
bats_tests = [
  'foo',
]

foreach bats_test : bats_tests
  test(
    'runtest-' + bats_test,
    bats,
    args: [
      '--tap', join_paths(meson.current_source_dir(), bats_test + '.bats'),
    ],
    env: [
      'NO_VALGRIND_WRAP=y',
      'TEST_FOO=' + foo.full_path(),
    ],
    depends: [
      foo,
    ],
    protocol: 'tap',
    suite: ['run'],
  )
endforeach
