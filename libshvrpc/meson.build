libshvrpc_sources = [
  files(
    'rpcaccess.c',
    'rpcclient_connect.c',
    'rpcclient_serial.c',
    'rpcclient_stream.c',
    'rpcdir.c',
    'rpcerror.c',
    'rpchandler.c',
    'rpchandler_app.c',
    'rpchandler_login.c',
    'rpclogger.c',
    'rpcmsg_head.c',
    'rpcmsg_pack.c',
    'rpcmsg_request_id.c',
    'rpcurl.c',
    'strset.c',
  ),
  gperf.process('rpcaccess.gperf'),
  gperf.process('rpcdir_key.gperf'),
  gperf.process('rpchandler_app_method.gperf'),
  gperf.process('rpcurl_query.gperf'),
  gperf.process('rpcurl_scheme.gperf'),
]
if not isnuttx
  libshvrpc_sources += files('sha1/openssl.c')
else
  libshvrpc_sources += files('sha1/nuttx.c')
endif
libshvrpc_dependencies = [libshvcp_dep, openssl, uriparser]
libshvrpc_internal_includes = include_directories('.')

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


pkg_mod.generate(
  libshvrpc,
  libraries: [libshvcp],
  description: 'Silicon Heaven RPC',
)
