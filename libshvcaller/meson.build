libshvcaller_sources = files(
  'rpccall.c',
  'rpchandler_responses.c',
  'rpchandler_signals.c',
)
libshvcaller_dependencies = [libshvrpc_dep]

libshvcaller = library(
  'shvcaller',
  libshvcaller_sources,
  version: '0.0.0',
  dependencies: libshvcaller_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvcaller.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvcaller_headers, preserve_path: true)

libshvcaller_dep = declare_dependency(
  dependencies: libshvcaller_dependencies,
  include_directories: includes,
  link_with: libshvcaller,
)


pkg_mod.generate(
  libshvcaller,
  libraries: [libshvrpc],
  description: 'Silicon Heaven RPC utilities for callers',
)
