demo_client_sources = files('opts.c')
demo_client_dependencies = [libshvrpc_dep]


demo_client = executable(
  'shvc-demo-client',
  demo_client_sources + ['main.c'],
  dependencies: demo_client_dependencies,
  install: true,
)
