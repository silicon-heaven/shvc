demo_device_sources = files(
  'handler.c',
  'opts.c',
)
demo_device_dependencies = [libshvrpc_dep]


demo_device = executable(
  'demo-device',
  demo_device_sources + ['main.c'],
  dependencies: demo_device_dependencies,
  install: true,
)

demo_internal_includes = include_directories('.')
