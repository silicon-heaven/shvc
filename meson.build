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


add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-fms-extensions', language: 'c')
add_project_arguments(
  '-DPROJECT_VERSION="@0@"'.format(meson.project_version()),
  language: 'c',
)
add_project_arguments(
  '-DSHV_UNPACK_BUFSIZ=' + get_option('unpack_bufsiz').to_string(),
  language: 'c',
)
add_project_arguments(
  '-DSHV_PACK_BUFSIZ=' + get_option('pack_bufsiz').to_string(),
  language: 'c',
)


gperf = generator(find_program('gperf'),
  output: '@PLAINNAME@.h',
  arguments: ['@EXTRA_ARGS@', '--output-file=@OUTPUT@', '@INPUT@']
)

uriparser = dependency('liburiparser', version: '>= 0.9.0')


subdir('include')
subdir('libshvchainpack')
subdir('libshvrpc')


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
