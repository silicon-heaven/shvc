shvcp_sources = files('opts.c')
shvcp_dependencies = [libshvcaller_dep]


shvcp = executable(
  'shvcp',
  shvcp_sources + ['main.c'],
  dependencies: shvcp_dependencies,
  install: true,
)

shvcp_internal_includes = include_directories('.')
