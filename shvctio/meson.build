shvctio_sources = files('opts.c')
shvctio_dependencies = [libshvcaller_dep]


shvctio = executable(
  'shvctio',
  shvctio_sources + ['main.c'],
  dependencies: shvctio_dependencies,
  install: true,
)
