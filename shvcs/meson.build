shvcs_sources = files('opts.c')
shvcs_dependencies = [libshvcaller_dep]


shvcs = executable(
  'shvcs',
  shvcs_sources + ['main.c'],
  dependencies: shvcs_dependencies,
  install: true,
)
