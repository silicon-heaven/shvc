shvc_sources = files('opts.c')
shvc_dependencies = [libshvcaller_dep]


shvc = executable(
  'shvc',
  shvc_sources + ['main.c'],
  dependencies: shvc_dependencies,
  install: true,
)
