shvcp_sources = files('opts.c')
shvcp_dependencies = [libshvrpc_dep]

shvcp = executable(
  'shvcp',
  shvcp_sources + ['main.c'],
  dependencies: shvcp_dependencies,
  install: true,
)
