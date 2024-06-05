libshvcallee_sources = [
  files('rpchandler_device.c'),
  gperf.process('rpchandler_device_method.gperf'),
]

libshvcallee_dependencies = [libshvrpc_dep]

libshvcallee = library(
  'shvcallee',
  libshvcallee_sources,
  version: '0.0.0',
  dependencies: libshvcallee_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvcallee.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvcallee_headers, preserve_path: true)

libshvcallee_dep = declare_dependency(
  dependencies: libshvcallee_dependencies,
  include_directories: includes,
  link_with: libshvcallee,
)


pkg_mod.generate(
  libshvcallee,
  description: 'Silicon Heaven RPC utilities for callees',
)
