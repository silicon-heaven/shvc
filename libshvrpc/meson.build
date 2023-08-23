libshvrpc_sources = [
  files(
    'rpcclient.c',
    'rpcclient_datagram.c',
    'rpcclient_serial.c',
    'rpcclient_stream.c',
    'rpcmsg_access.c',
    'rpcmsg_meta.c',
    'rpcmsg_pack.c',
    'rpcurl.c',
  ),
  gperf.process('rpcmsg_access.gperf'),
  gperf.process('rpcurl_query.gperf'),
  gperf.process('rpcurl_scheme.gperf'),
]
libshvrpc_dependencies = [libshvchainpack_dep, uriparser]

libshvrpc = library(
  'shvrpc',
  libshvrpc_sources,
  version: '0.0.0',
  dependencies: libshvrpc_dependencies,
  include_directories: includes,
  link_args: '-Wl,--version-script=' + join_paths(
    meson.current_source_dir(),
    'libshvrpc.version',
  ),
  install: not meson.is_subproject(),
)
install_headers(libshvrpc_headers)

libshvrpc_dep = declare_dependency(
  dependencies: libshvrpc_dependencies,
  include_directories: includes,
  link_with: libshvrpc,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(libshvrpc, description: 'Silicon Heaven RPC')
