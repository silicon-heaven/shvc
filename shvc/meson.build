shvc_sources = files('opts.c')
shvc_dependencies = [libshvrpc_dep]


if isnuttx
  shvc = static_library(
    'shvc',
    shvc_sources + ['main.c'],
    dependencies: shvc_dependencies,
    install: not meson.is_subproject(),
    c_args: '-Dmain=shvc_main',
  )
else
  shvc = executable(
    'shvc',
    shvc_sources + ['main.c'],
    dependencies: shvc_dependencies,
    install: true,
  )
endif

shvc_internal_includes = include_directories('.')
