demo_client_sources = files(
  'opts.c',
  'rpc_connection.c',
  'rpc_device_operations.c',
  'rpc_error_logging.c',
  'rpc_request.c',
  'utils.c',
)
demo_client_dependencies = [libshvrpc_dep]


demo_client = executable(
  'demo-client',
  demo_client_sources + ['main.c'],
  dependencies: demo_client_dependencies,
  install: true,
)

demo_internal_includes = include_directories('.')
