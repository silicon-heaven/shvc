libshvcp_sources = files(
  'chainpack_pack.c',
  'chainpack_unpack.c',
  'common.c',
  'cp_pack.c',
  'cp_unpack.c',
  'cp_unpack_pack.c',
  'cpdatetime.c',
  'cpdecimal.c',
  'cpitem.c',
  'cpon_pack.c',
  'cpon_unpack.c',
)
libshvcp_dependencies = [m]

libshvcp = library(
  'shvcp',
  libshvcp_sources,
  version: '0.0.0',
  dependencies: libshvcp_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvcp.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvcp_headers, preserve_path: true)

libshvcp_dep = declare_dependency(
  dependencies: libshvcp_dependencies,
  include_directories: includes,
  link_with: libshvcp,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(
  libshvcp,
  description: 'Silicon Heaven Chainpack packer and unpacker',
)
