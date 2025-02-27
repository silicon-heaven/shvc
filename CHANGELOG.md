# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- `cptstodt` and `cpdttots` functions to `cp.h` to work with `struct timespec`.
- `cpdecexp` function and `cpitod` macro to `cp.h` for `struct cpdecimal`.

### Changed
- `fetch` method in RPCHistory now gives the application starting index
  and the number of records to be fetched, allowing better optimization
  from the application.

### Removed
- No longer valid RPC error codes (they were removed from standard).
- `cpdecimal` and `cpdeccmp` as having invalid or no implementation.

### Fixed
- RPC File's methods `sha1` and `crc` now correctly accept no parameter and
  single item in the list.


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
