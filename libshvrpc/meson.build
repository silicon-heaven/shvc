libshvrpc_sources = [
  files(
    'rpchandler/app.c',
    'rpchandler/device.c',
    'rpchandler/file.c',
    'rpchandler/login.c',
    'rpchandler/responses.c',
    'rpchandler/signals.c',
    'rpctransport/pipe.c',
    'rpctransport/tcp.c',
    'rpctransport/tty.c',
    'rpctransport/unix.c',
    'crc32.c',
    'rpcaccess.c',
    'rpcalerts.c',
    'rpccall.c',
    'rpcclient_stream.c',
    'rpcdir.c',
    'rpcerror.c',
    'rpcfile.c',
    'rpchandler.c',
    'rpclogger.c',
    'rpclogin.c',
    'rpcmsg_head.c',
    'rpcmsg_pack.c',
    'rpcmsg_request_id.c',
    'rpcri.c',
    'rpcurl.c',
    'rpcurl_new.c',
    'strset.c',
  ),
  gperf.process('rpcaccess.gperf'),
  gperf.process('rpcdir_key.gperf'),
  gperf.process('rpchandler/app_method.gperf'),
  gperf.process('rpchandler/device_method.gperf'),
  gperf.process('rpchandler/file_method.gperf'),
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
