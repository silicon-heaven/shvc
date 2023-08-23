shvbroker_sources = files('config.c')
shvbroker_dependencies = [libshvbroker_dep, argp]


if isnuttx
  shvbroker = static_library(
    'shvbroker',
    shvbroker_sources + ['main.c'],
    dependencies: shvbroker_dependencies,
    install: not meson.is_subproject(),
    c_args: '-Dmain=shvbroker_main',
  )
else
  shvbroker = executable(
    'shvbroker',
    shvbroker_sources + ['main.c'],
    dependencies: shvbroker_dependencies,
    install: true,
  )
endif

shvbroker_internal_includes = include_directories('.')
