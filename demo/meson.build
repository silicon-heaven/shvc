demo_device_sources = files(
  'device_handler.c',
  'opts.c',
)
demo_device_dependencies = [libshvrpc_dep]


if isnuttx
  demo_device = static_library(
    'demo-device',
    demo_device_sources + ['main.c'],
    dependencies: demo_device_dependencies,
    install: not meson.is_subproject(),
    c_args: '-Dmain=demo_device_main',
  )
else
  demo_device = executable(
    'demo-device',
    demo_device_sources + ['main.c'],
    dependencies: demo_device_dependencies,
    install: true,
  )
endif

demo_internal_includes = include_directories('.')
