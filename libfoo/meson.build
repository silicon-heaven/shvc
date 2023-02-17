libfoo_sources = [
  files('count.c'),
  gperf.process('match.gperf'),
]
libfoo_dependencies = []

libfoo = library(
  'foo',
  libfoo_sources,
  version: '0.0.0',
  dependencies: libfoo_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libfoo.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libfoo_headers)

libfoo_dep = declare_dependency(
  dependencies: libfoo_dependencies,
  include_directories: includes,
  link_with: libfoo,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(libfoo, description: 'Library counting foos..')
