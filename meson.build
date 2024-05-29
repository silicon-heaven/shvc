project(
  'template-c',
  'c',
  version: run_command(find_program('bash'), './.version.sh', check: true).stdout().strip(),
  license: 'GPL-3.0-or-later',
  default_options: 'c_std=c17',
  meson_version: '>=0.57.0',
)
cc = meson.get_compiler('c')
isnuttx = cc.get_define('__NuttX__') != '' # Won't be defined outside NuttX
pkg_mod = import('pkgconfig')

if not isnuttx
  add_project_arguments('-D_GNU_SOURCE', language: 'c')
endif
add_project_arguments('-DPROJECT_VERSION="@0@"'.format(meson.project_version()), language: 'c')


if not isnuttx
  argp = cc.has_function('argp_parse') ? declare_dependency() : cc.find_library('argp')
else
  argp = declare_dependency()
endif

gperf = generator(find_program('gperf'),
  output: '@PLAINNAME@.h',
  arguments: ['@EXTRA_ARGS@', '--output-file=@OUTPUT@', '@INPUT@']
)


subdir('include')
subdir('libfoo')

foo_opt = get_option('foo')
if foo_opt
  subdir('src')
endif


test_buildtypes = ['debug', 'debugoptimized']
tests_opt = get_option('tests')
if (
  not isnuttx
  and not meson.is_subproject()
  and (
    tests_opt.enabled()
    or (tests_opt.auto()
    and get_option('buildtype') in test_buildtypes)
  )
)
  subdir('tests')
endif
