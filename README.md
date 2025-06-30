# Silicon Heaven in C

This provides implementation of [Silicon Heaven communication
protocol](https://silicon-heaven.github.io/shv-doc/) in C.

* [📃 Sources](https://gitlab.com/silicon-heaven/shvc)
* [⁉️ Issue tracker](https://gitlab.com/silicon-heaven/shvc/-/issues)
* [📕 Documentation](https://silicon-heaven.gitlab.io/shvc/)

The code is divided to the separate libraries and applications for easier
management. Every library or application has its own specific goal and together
they provide complete SHV RPC toolkit.

* **libshvcp** provides packing and unpacking facilities for CPON and ChainPack.
  The implementation is based on streams (`FILE`) and packing and unpacking can
  be done without `malloc()`.
* **libshvrpc** provides a complete facilities to receive and send SHV RPC
  messages as well as more advanced constructs to manage them (`rpchandler`).
* **libshvbroker** is a library that implements SHV RPC Broker and thus broker's
  functionality can be combined with other to create combined and custom
  applications providing SHV RPC broker functionality.
* **libshvhistory** is a library that provides implementation for history
  application. It can be used to create logging facility that is part of SHV
  RPC.
* **shvc** is CLI tool to call SHV RPC methods.
* **shvcs** is CLI tool for receiving the SHV RPC signals.
* **shvcp** is CLI tool for working with file nodes in SHV RPC. (NOT IMPLEMENTED
  YET)
* **shvctio** is CLI tool for attaching console to the exchange nodes in SHV
  RPC. (NOT IMPLEMENTED YET)
* **shvcbroker** is standalone SHV RPC Broker application.

## Dependencies

* [Meson build system](https://mesonbuild.com/)
* [gperf](https://www.gnu.org/software/gperf)
* On non-glibc [argp-standalone](http://www.lysator.liu.se/~nisse/misc)

For tests:

* [check-suite](https://gitlab.com/Cynerd/check-suite)
* [pytest](https://docs.pytest.org/)
* [pytest-tap](https://github.com/python-tap/pytest-tap)
* [pyshv](https://gitlab.com/silicon-heaven/pyshv)
* Optionally [valgrind](http://www.valgrind.org)

For code coverage report:

* [gcovr](https://gcovr.com)


## Compilation

To compile this project you have to run:

```console
$ meson setup builddir
$ meson compile -C builddir
```

Subsequent installation can be done with `meson install -C builddir`.

## Documentation

The documentation can be built using `sphinx-build docs html`.

When you are writing documentation it is handy to use
[Sphinx-autobuild](https://pypi.org/project/sphinx-autobuild/).


## Running tests

This project contains basic tests in directory tests.

To run tests you have to either use `debug` build type (which is commonly the
default for meson) or explicitly enable them using `meson configure
-Dtests=enabled builddir`. To execute all tests run:

```console
$ meson test -C builddir
```

You can also run tests with [Valgrind](http://www.valgrind.org) tools
`memcheck`, `helgrind`, and `drd`:

```console
$ meson test -C builddir --setup memcheck
```

### Code coverage report

There is also possibility to generate code coverage report from test cases. To
do so you need gcovr and then you can run:

```console
$ meson setup -Db_coverage=true builddir
$ meson test -C builddir
$ ninja -C builddir coverage-html
```

The coverage report is generated in directory:
`builddir/meson-logs/coveragereport`.

## Linting the code

The code can also be linted if
[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) is installed. To run
it you can do:

```console
$ meson setup builddir
$ ninja -C builddir clang-tidy
```

The other linters are also used for other files in this project; please inspect
the `.gitlab-ci.yml` file for `.linter` jobs.

## Formating the code

The code should be automatically formatted with
[clang-format](https://clang.llvm.org/docs/ClangFormat.html). The Meson build
files with [muon](https://muon.build/).

```console
$ meson setup builddir
$ ninja -C builddir clang-format
$ git ls-files '**/meson.build' meson_options.txt | xargs muon fmt -c .muon_fmt.ini -i
```

The other formatters are also used for other files in this project; please
inspect the `.gitlab-ci.yml` file for `.style` jobs.

## Using Nix development environment

The build environment, that is all necessary software required to build, lint
and test the project can be provided using Nix. To use it you have to install
it, please refer to the [Nix's documentation for
that](https://nixos.org/download.html).

Once you have Nix you can use it to enter development environment. Navigate to
the project's directory and run `nix develop`.
