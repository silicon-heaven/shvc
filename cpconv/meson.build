cpconv_sources = files('opts.c')
cpconv_dependencies = [libshvchainpack_dep, argp]


if isnuttx
  cpconv = static_library(
    'cpconv',
    cpconv_sources + ['main.c'],
    dependencies: cpconv_dependencies,
    install: not meson.is_subproject(),
    c_args: '-Dmain=cpconv_main',
  )
else
  cpconv = executable(
    'cpconv',
    cpconv_sources + ['main.c'],
    dependencies: cpconv_dependencies,
    install: true,
  )
endif

cpconv_internal_includes = include_directories('.')
