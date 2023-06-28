# Silicon Heaven in C

This provides implementation of Silicon Heaven communication protocol in C. This
implementation is based on top of threads where there is a dedicated thread that
manages the read while all threads can negotiate for write access.

## Dependencies

* [Meson build system](https://mesonbuild.com/)
* [gperf](https://www.gnu.org/software/gperf)
* On non-glibc [argp-standalone](http://www.lysator.liu.se/~nisse/misc)

For tests:

* [check-suite](https://gitlab.com/Cynerd/check-suite)
* [bats](https://bats-core.readthedocs.io/en/stable/index.html)
* Optionally [valgrind](http://www.valgrind.org)

For code coverage report:

* [lcov](http://ltp.sourceforge.net/coverage/lcov.php)

For linting:

* [cppcheck](https://github.com/danmar/cppcheck)
* [flawfinder](https://dwheeler.com/flawfinder/)


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

You can also run tests with Valgrind tool such as `memcheck`:

```console
$ VALGRIND=memcheck meson test -C builddir
```

### Code coverage report

There is also possibility to generate code coverage report from test cases. To
do so you can run:

```console
$ meson setup -Db_coverage=true builddir
$ meson test -C builddir
$ ninja -C builddir coverage-html
```

The coverage report is generated in directory:
`builddir/meson-logs/coveragereport`.

## Linting the code

The code can also be linted if linters are installed. There are two linter
supported at the moment. There is `cppcheck` and `flawfinder`. To run them you
can do:

```console
$ meson setup builddir
$ meson compile -C builddir cppcheck
$ meson compile -C builddir flawfinder
```

## Using Nix development environment

The build environment, that is all necessary software required to build, lint
and test the project can be provided using Nix. To use it you have to install
it, please refer to the [Nix's documentation for
that](https://nixos.org/download.html).

Once you have Nix you can use it to enter development environment. Navigate to
the project's directory and run `nix develop`.
