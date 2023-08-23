project(
  'shvc',
  'c',
  version: files('version'),
  license: 'GPL-3.0-or-later',
  default_options: 'c_std=c17',
  meson_version: '>=0.57.0',
)
cc = meson.get_compiler('c')
isnuttx = (
  cc.get_define('__NuttX__') != '' # Won't be defined outside NuttX
)
fs = import('fs')
cmake = import('cmake')


add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-fms-extensions', language: 'c')
add_project_arguments(
  '-DPROJECT_VERSION="@0@"'.format(meson.project_version()),
  language: 'c',
)


if not isnuttx
  m = cc.find_library('m', required : false)
  argp = cc.has_function('argp_parse') ? declare_dependency() : cc.find_library('argp')
  uriparser = dependency('liburiparser', version: '>= 0.9.0')
else
  m = declare_dependency()
  argp = declare_dependency()
  uriparser_var = cmake.subproject_options()
  uriparser_var.add_cmake_defines({
    'URIPARSER_BUILD_TESTS': false,
    'URIPARSER_BUILD_DOCS': false,
    'URIPARSER_BUILD_TOOLS': false,
    'URIPARSER_BUILD_WCHAR_T': false,
  })
  uriparser_proj = cmake.subproject('uriparser', options: uriparser_var)
  uriparser = uriparser_proj.dependency('uriparser')
endif


gperf = generator(find_program('gperf'),
  output: '@PLAINNAME@.h',
  arguments: ['@EXTRA_ARGS@', '--output-file=@OUTPUT@', '@INPUT@']
)


subdir('include')
subdir('libshvchainpack')
subdir('libshvrpc')
subdir('libshvbroker')

cpconv_opt = get_option('cpconv')
if cpconv_opt
  subdir('cpconv')
endif
shvbroker_opt = get_option('shvbroker')
if shvbroker_opt
  subdir('shvbroker')
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


cppcheck = find_program('cppcheck', required: false)
if cppcheck.found()
  run_target(
    'cppcheck',
    command: [
      cppcheck,
      '--project=' + join_paths(
        meson.project_build_root(),
        'compile_commands.json',
      ),
      '--suppress=*:*subprojects*',
    ],
  )
endif

flawfinder = find_program('flawfinder', required: false)
if flawfinder.found()
  run_target(
    'flawfinder',
    command: [flawfinder, '--error-level=5', meson.project_source_root()],
  )
endif
