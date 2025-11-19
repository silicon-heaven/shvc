valgrind = find_program('valgrind', required: false)
if valgrind.found()
  bash = find_program('bash')
  valgrind_wrapper = [bash, meson.current_source_dir() + '/valgrind-wrapper.sh', valgrind]
  add_test_setup(
    'memcheck',
    exe_wrapper: valgrind_wrapper + 'memcheck',
    timeout_multiplier: 3,
  )
  add_test_setup(
    'helgrind',
    exe_wrapper: valgrind_wrapper + 'helgrind',
    timeout_multiplier: 3,
  )
  add_test_setup(
    'drd',
    exe_wrapper: valgrind_wrapper + 'drd',
    timeout_multiplier: 4,
  )
endif

subdir('unit')
subdir('run')
