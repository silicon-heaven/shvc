libshvchainpack_sources = files(
  'chainpack_pack.c',
  'chainpack_unpack.c',
  'cp_pack.c',
  'cp_unpack.c',
  'cpdatetime.c',
  'cpdecimal.c',
  'cpon_pack.c',
  'cpon_unpack.c',
)
libshvchainpack_dependencies = [m]

libshvchainpack = library(
  'shvchainpack',
  libshvchainpack_sources,
  version: '0.0.0',
  dependencies: libshvchainpack_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvchainpack.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvchainpack_headers)

libshvchainpack_dep = declare_dependency(
  dependencies: libshvchainpack_dependencies,
  include_directories: includes,
  link_with: libshvchainpack,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(
  libshvchainpack,
  description: 'Silicon Heaven Chainpack packer and unpacker',
)
