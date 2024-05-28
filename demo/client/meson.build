demo_client_sources = files('opts.c')
demo_client_dependencies = [libshvcaller_dep]


demo_client = executable(
  'demo-client',
  demo_client_sources + ['main.c'],
  dependencies: demo_client_dependencies,
  install: true,
)

demo_internal_includes = include_directories('.')
