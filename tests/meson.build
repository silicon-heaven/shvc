bash = find_program('bash')
test_driver = meson.current_source_dir() + '/test-driver.sh'

pyshvbroker = find_program('pyshvbroker')

add_test_setup('memcheck', env: {'VALGRIND': 'memcheck'})
add_test_setup('helgrind', env: {'VALGRIND': 'helgrind'})
add_test_setup('drd', env: {'VALGRIND': 'drd'})

subdir('unit')
subdir('run')
