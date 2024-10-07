includes = include_directories('.')

libshvcp_headers = files(
  'shv/chainpack.h',
  'shv/cp.h',
  'shv/cp_pack.h',
  'shv/cp_tools.h',
  'shv/cp_unpack.h',
)
libshvrpc_headers = files(
  'shv/crc32.h',
  'shv/rpcaccess.h',
  'shv/rpcalerts.h',
  'shv/rpccall.h',
  'shv/rpcclient.h',
  'shv/rpcclient_stream.h',
  'shv/rpcdir.h',
  'shv/rpcerror.h',
  'shv/rpcfile.h',
  'shv/rpchandler.h',
  'shv/rpchandler_app.h',
  'shv/rpchandler_device.h',
  'shv/rpchandler_file.h',
  'shv/rpchandler_impl.h',
  'shv/rpchandler_login.h',
  'shv/rpchandler_responses.h',
  'shv/rpchandler_signals.h',
  'shv/rpclogger.h',
  'shv/rpclogin.h',
  'shv/rpcmsg.h',
  'shv/rpcri.h',
  'shv/rpcserver.h',
  'shv/rpctransport.h',
  'shv/rpcurl.h',
  'shv/sha1.h',
  'shv/version.h',
)
libshvbroker_headers = files('shv/rpcbroker.h')

libshvhistory_headers = files(
  'shv/rpchandler_files.h',
  'shv/rpchandler_records.h',
  'shv/rpchistory.h',
)

configure_file(output: 'shvc_config.h', configuration: conf)
