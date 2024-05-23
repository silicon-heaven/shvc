shvc_sources = files('opts.c')
shvc_dependencies = [libshvclient_dep]


shvc = executable(
  'shvc',
  shvc_sources + ['main.c'],
  dependencies: shvc_dependencies,
  install: true,
)

shvc_internal_includes = include_directories('.')
