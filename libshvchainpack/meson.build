libshvchainpack_sources = files(
  'chainpack.c',
  'chainpack_pack.c',
  'chainpack_unpack.c',
  'cpcp.c',
  'cpcp_convert.c',
  'cpon.c',
)

libshvchainpack = library(
  'shvchainpack',
  libshvchainpack_sources,
  version: '0.0.0',
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvchainpack.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvchainpack_headers)

libshvchainpack_dep = declare_dependency(
  include_directories: includes,
  link_with: libshvchainpack,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(
  libshvchainpack,
  description: 'Silicon Heaven Chainpack packer and unpacker',
)
