libshvhistory_sources = files(
  'rpchandler_history.c',
  'rpchistory.c',
)
libshvhistory_dependencies = [libshvrpc_dep]

libshvhistory = library(
  'shvhistory',
  libshvhistory_sources,
  version: '0.0.0',
  dependencies: libshvhistory_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvhistory.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvhistory_headers, preserve_path: true)

libshvhistory_dep = declare_dependency(
  dependencies: libshvhistory_dependencies,
  include_directories: includes,
  link_with: libshvhistory,
)


pkg_mod.generate(
  libshvhistory,
  description: 'Silicon Heaven History Implementation',
)
