project(
  'shvc',
  'c',
  version: run_command(find_program('bash'), './.version.sh', check: true).stdout().strip(),
  license: 'GPL-3.0-or-later',
  default_options: 'c_std=gnu2x',
  meson_version: '>=0.63.0',
)
cc = meson.get_compiler('c')
isnuttx = cc.get_define('__NuttX__') != '' # Won't be defined outside NuttX
pkg_mod = import('pkgconfig')
cmake = import('cmake')
python = import('python')

add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-DPROJECT_VERSION="@0@"'.format(meson.project_version()), language: 'c')

conf = configuration_data()
conf.set('SHVC_TCP', get_option('tcp'), description:'If TCP/IP should be compiled in.')
conf.set('SHVC_CRYPTORAND', get_option('cryptorand'), description:'If getrandom should be used instead of rand.')
conf.set_quoted('SHVC_DEFAULT_URL', get_option('default-url'), description:'The default connection URL.')

if not isnuttx
  m = cc.find_library('m', required : false)
  openssl = dependency('openssl', version: '>=3.0.0')
  uriparser = dependency('liburiparser', version: '>= 0.9.0')
else
  m = declare_dependency()
  openssl = declare_dependency()
  uriparser_var = cmake.subproject_options()
  uriparser_var.add_cmake_defines({
    'CMAKE_TRY_COMPILE_TARGET_TYPE': 'STATIC_LIBRARY',
    'BUILD_SHARED_LIBS': false,
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
subdir('libshvcp')
subdir('libshvrpc')
subdir('libshvbroker')
subdir('libshvhistory')

shvc_opt = get_option('shvc')
if shvc_opt
  subdir('shvc')
endif
shvcs_opt = get_option('shvcs')
if shvcs_opt
  subdir('shvcs')
endif
shvcp_opt = get_option('shvcp')
if shvcp_opt
  subdir('shvcp')
endif
shvctio_opt = get_option('shvctio')
if shvctio_opt
  subdir('shvctio')
endif
shvcbroker_opt = get_option('shvcbroker')
if shvcbroker_opt
  subdir('shvcbroker')
endif
demo_device_opt = get_option('demo-device')
demo_client_opt = get_option('demo-client')
subdir('demo')


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
