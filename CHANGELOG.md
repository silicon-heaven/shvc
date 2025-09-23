# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Fixed
- `rpcmsg.cids` has now `intmax_t*` type instead of `long long*`


## [0.6.0] - 2025-09-22
### Added
- Pack functions for file based logging
- `cpitem_extract_decimal_int`, `cp_pack_decimal_int`, and
  `cp_unpack_decimal_int` to simplify working with decimals represented in the
  code as plain integers with fixed multiplication.
- Fields `ref` and `id` to `.history/**/.records/*:fetch` method's response.
- `RPCDIR_F_ISUPDATABLE`
- `RPCDIR_F_LONG_EXECUTION`

### Changed
- Snapshot is removed from `getLog` call and provided in a separate
  `getSnapshot` method.
- `RPCDIR_F_CLIENTID_REQUIRED` renamed to `RPDIR_F_USERID_REQUIRED` to be
  consistent with SHV standard

### Fixed
- Invalid memory access in Broker caused by wrong bits count calculation.


## [0.5.0] - 2025-05-14
### Added
- `cp_unpack_decimal` to simplify `cpdecimal` unpacking.
- `rpclogger` callback `rpclogger_allowed_t` that determines if logging
  should be performed and thus optimizes unnecessary `CPON` packing.

### Changed
- Renamed `mantisa` field to correct `mantissa` in `cpdecimal` structure.
- `cpon_pack` packer now uses faster `fputc`/`fwrite` instead of `printf`.
- `CPON` packs `double` in a scientific notation with `p`.
- `rpclogger_new` now takes `rpclogger_funcs` structure as an argument
  instead of just `rpclogger_func_t`.
- Removed `snapshot` field from `getLog` request and converted `Map` to
  `iMap` per updated specification.


## [0.4.0] - 2025-04-08
### Added
- `cptstodt` and `cpdttots` functions to `cp.h` to work with `struct timespec`.
- `cpdecexp` function and `cpitod` macro to `cp.h` for `struct cpdecimal`.
- `-s` argument to the `shvc` application to allow user ID to be initialized

### Changed
- `fetch` method in RPCHistory now gives the application starting index
  and the number of records to be fetched, allowing better optimization
  from the application.
- Parameter and result type hints of the builtin method descriptions now follow
  RPC type hints defined in SHV standard.
- RPC client now uses standard `FILE` buffer for writing message with serial
  protocol and thus improving the performace.
- CPON and ChainPack packing and unpacking now uses unlocked read/write
  functions variants. This improves performace.

### Removed
- No longer valid RPC error codes (they were removed from standard).
- `cpdecimal` and `cpdeccmp` as having invalid or no implementation.

### Fixed
- RPC File's methods `sha1` and `crc` now correctly accept no parameter and
  single item in the list.
- Method `get` on `.device/alerts` path now correctly handles integer
  parameter.
- Double memory free error in rpccall.


## [0.3.0] - 2025-01-16
### Added
- Support for extracting unsigned SHV integer to the signed one and vice versa.
- Support for `getLog` method in RPCHistory.

### Changed
- `long long` and `unsigned long long` are replaced with `intmax_t` and
  `uintmax_t` respectively in the `struct cpitem` and `struct cpdecimal`
- History handlers and `rpchandler_records` and `rpchandler_files` are merged
  into a signle handler `rpchandler_history`.

### Fixed
- `cpdatetime` now correctly fills in `offutc` field.
- Chainpack unpack of more than 32 bit signed negative integers
- Possible memory corruption when using `rpchandler_ls_result_vfmt`
- Possible deadlock in the broker when pipes are used


## [0.2.0] - 2024-10-24
### Added
- Support for device alerts
- Ability to set default message's meta limits at compile time
- Signaling if client supports connection tracking
- `rpchandler_msg_valid_getparam` utility function

### Fixed
- Serial client disconnect now no longer blocks when output buffer is full


## [0.1.0] - 2024-09-23
### Added
- Initial implementation
