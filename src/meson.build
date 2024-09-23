foo_sources = files('config.c')
foo_dependencies = [libfoo_dep, argp]

foo = executable(
  'foo',
  foo_sources + ['main.c'],
  dependencies: foo_dependencies,
  install: true,
)

foo_internal_includes = include_directories('.')
