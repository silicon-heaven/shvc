unittest_utils_src = files(
  'broker.c',
  'packstream.c',
  'tmpdir.c',
  'unpack.c',
)
unittest_utils_includes = include_directories('.')
unittest_utils_c_args = [
  '-DPYSHVBROKER="' + pyshvbroker.full_path() + '"',
]
