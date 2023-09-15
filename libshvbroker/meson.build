libshvbroker_sources = files()

libshvbroker = library(
  'shvbroker',
  libshvbroker_sources,
  version: '0.0.0',
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvbroker.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvbroker_headers, preserve_path: true)

libshvbroker_dep = declare_dependency(
  include_directories: includes,
  link_with: libshvbroker,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(
  libshvbroker,
  description: 'Silicon Heaven Broker',
)
