shvcbroker_sources = files('config.c')
shvcbroker_dependencies = [libshvbroker_dep]


shvcbroker = executable(
  'shvcbroker',
  shvcbroker_sources + ['main.c'],
  dependencies: shvcbroker_dependencies,
  install: true,
)

shvcbroker_internal_includes = include_directories('.')
