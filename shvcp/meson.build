shvcsub_sources = files('opts.c')
shvcsub_dependencies = [libshvrpc_dep]


shvcsub = executable(
  'shvcsub',
  shvcsub_sources + ['main.c'],
  dependencies: shvcsub_dependencies,
  install: true,
)

shvcsub_internal_includes = include_directories('.')
