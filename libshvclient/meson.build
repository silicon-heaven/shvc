libshvclient_sources = files(
  'rpccall.c',
  'rpchandler_responses.c',
)
libshvclient_dependencies = [libshvrpc_dep]

libshvclient = library(
  'shvclient',
  libshvclient_sources,
  version: '0.0.0',
  dependencies: libshvclient_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvclient.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvclient_headers, preserve_path: true)

libshvclient_dep = declare_dependency(
  dependencies: libshvclient_dependencies,
  include_directories: includes,
  link_with: libshvclient,
)


pkg_mod.generate(
  libshvclient,
  libraries: [libshvrpc],
  description: 'Silicon Heaven RPC client utilities',
)
