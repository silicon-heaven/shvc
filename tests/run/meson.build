bats = find_program('bats')

bats_tests = []

foreach bats_test : bats_tests
  test(
    'runtest-' + bats_test,
    bash,
    args: [
      test_driver,
      '-n',
      bats.full_path(),
      '--tap',
      join_paths(meson.current_source_dir(), bats_test + '.bats'),
    ],
    env: [],
    protocol: 'tap',
    depends: foo,
  )
endforeach
