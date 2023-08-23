check_suite = dependency('check-suite')
obstack = (
  cc.has_function('obstack_free') ? declare_dependency() : cc.find_library(
    'obstack'
  )
)

unittests_env = [
  'CK_TAP_LOG_FILE_NAME=/dev/stdout',
  'CK_VERBOSITY=silent',
]

subdir('utils')
subdir('libshvchainpack')
subdir('libshvrpc')
