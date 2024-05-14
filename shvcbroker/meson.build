shvbroker_sources = files('config.c')
shvbroker_dependencies = [libshvbroker_dep]


shvbroker = executable(
  'shvbroker',
  shvbroker_sources + ['main.c'],
  dependencies: shvbroker_dependencies,
  install: true,
)

shvbroker_internal_includes = include_directories('.')
