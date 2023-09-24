libshvrpc_sources = [
  files(
    'rpcapp.c',
    'rpcclient_connect.c',
    'rpcclient_datagram.c',
    'rpcclient_log.c',
    'rpcclient_login.c',
    'rpcclient_serial.c',
    'rpcclient_stream.c',
    'rpchandler.c',
    'rpcmsg_access.c',
    'rpcmsg_head.c',
    'rpcmsg_pack.c',
    'rpcrespond.c',
    'rpcurl.c',
  ),
  gperf.process('rpcmsg_access.gperf'),
  gperf.process('rpcurl_query.gperf'),
  gperf.process('rpcurl_scheme.gperf'),
]
libshvrpc_dependencies = [libshvchainpack_dep, openssl, uriparser]

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
install_headers(libshvrpc_headers, preserve_path: true)

libshvrpc_dep = declare_dependency(
  dependencies: libshvrpc_dependencies,
  include_directories: includes,
  link_with: libshvrpc,
)


pkg_mod = import('pkgconfig')
pkg_mod.generate(
  libshvrpc,
  libraries: [libshvchainpack],
  description: 'Silicon Heaven RPC',
)
