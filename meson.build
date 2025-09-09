project(
  'shvc',
  'c',
  version: run_command(find_program('bash'), './.version.sh', check: true).stdout().strip(),
  license: 'MIT',
  default_options: 'c_std=gnu2x',
  meson_version: '>=0.63.0',
)
cc = meson.get_compiler('c')
isnuttx = cc.get_define('__NuttX__') != '' # Won't be defined outside NuttX
pkg_mod = import('pkgconfig')
cmake = import('cmake')
pymod = import('python')

add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-DPROJECT_VERSION="@0@"'.format(meson.project_version()), language: 'c')

conf = configuration_data()
conf.set('SHVC_TCP', get_option('tcp'), description:'If TCP/IP should be compiled in.')
conf.set('SHVC_CRYPTORAND', get_option('cryptorand'), description:'If getrandom should be used instead of rand.')
conf.set_quoted('SHVC_DEFAULT_URL', get_option('default-url'), description:'The default connection URL.')
conf.set( 'SHVC_FILE_MAXWRITE', get_option('file_maxwrite'), description:'Maximal size in bytes of single write operation for file nodes')
conf.set('SHVC_GETLOG_LIMIT', get_option('getlog_limit'), description:'Maximal number of records that can be obtained with getLog method.')
conf.set('SHVC_NUTTXCAN', get_option('nuttxcan'), description:'Use NuttX specific CAN interface instead of SocketCAN.')
conf.set('SHVC_PATH_LIMIT', get_option('path_limit'), description:'Default limit imposed on the path size. You can set it to `-1` for unconstrained allocation.')
conf.set('SHVC_NAME_LIMIT', get_option('name_limit'), description:'Default limit imposed on the method, source, and signal size.')
conf.set('SHVC_USERID_LIMIT', get_option('userid_limit'), description:'Default limit imposed the on the userid size.')

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
if shvc_opt.enabled() or (shvc_opt.auto() and not meson.is_subproject())
  subdir('shvc')
endif
shvcs_opt = get_option('shvcs')
if shvcs_opt.enabled() or (shvcs_opt.auto() and not meson.is_subproject())
  subdir('shvcs')
endif
shvcp_opt = get_option('shvcp')
if shvcp_opt.enabled() or (shvcp_opt.auto() and not meson.is_subproject())
  subdir('shvcp')
endif
shvctio_opt = get_option('shvctio')
if shvctio_opt.enabled() or (shvctio_opt.auto() and not meson.is_subproject())
  subdir('shvctio')
endif
shvcbroker_opt = get_option('shvcbroker')
if shvcbroker_opt.enabled() or (shvcbroker_opt.auto() and not meson.is_subproject())
  subdir('shvcbroker')
endif
demo_device_opt = get_option('demo-device')
demo_client_opt = get_option('demo-client')
demo_history_opt = get_option('demo-history')
subdir('demo')


test_buildtypes = ['debug', 'debugoptimized']
tests_opt = get_option('tests')
if (
  not isnuttx
  and not meson.is_subproject()
  and (
    tests_opt == 'enabled'
    or (tests_opt == 'auto' and get_option('buildtype') in test_buildtypes)
  )
)
  subdir('tests')
endif
