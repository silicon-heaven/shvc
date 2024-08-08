libshvbroker_sources = [
  files(
    'access_stage.c',
    'api.c',
    'api_login.c',
    'mount.c',
    'role.c',
    'rpc_stage.c',
    'rpcbroker.c',
    'rpcbroker_run.c',
    'subscription.c',
    'util.c',
  ),
  gperf.process('api_broker_method.gperf'),
  gperf.process('api_current_client_method.gperf'),
]
libshvbroker_dependencies = [libshvrpc_dep]
libshvbroker_internal_includes = include_directories('.')

libshvbroker = library(
  'shvbroker',
  libshvbroker_sources,
  version: '0.0.0',
  dependencies: libshvbroker_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvbroker.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvbroker_headers, preserve_path: true)

libshvbroker_dep = declare_dependency(
  dependencies: libshvbroker_dependencies,
  include_directories: includes,
  link_with: libshvbroker,
)


pkg_mod.generate(
  libshvbroker,
  libraries: [libshvrpc],
  description: 'Silicon Heaven Broker',
)
