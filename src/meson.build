foo_sources = files('config.c')
foo_dependencies = [libfoo_dep, argp]


if isnuttx
  shvtreecdemo_client = static_library(
    'foo',
    foo_sources + ['main.c'],
    dependencies: foo_dependencies,
    install: not meson.is_subproject(),
    c_args: '-Dmain=foo_main',
  )
else
  foo = executable(
    'foo',
    foo_sources + ['main.c'],
    dependencies: foo_dependencies,
    install: true,
  )
endif

foo_internal_includes = include_directories('.')
