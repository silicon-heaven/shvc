# Project Template C

This project provides common base for C projects. It is intended to be merged to
the new projects to introduce the common build infrastructure and testing. This
description should be changed to the description of your project. Do NOT
forget to modify it together with the title of this file, board info and other
resources mentioned in this readme file.

## Dependencies

* [Meson build system](https://mesonbuild.com/)
* [gperf](https://www.gnu.org/software/gperf)
* On non-glibc [argp-standalone](http://www.lysator.liu.se/~nisse/misc)
* [check-suite](https://gitlab.com/Cynerd/check-suite) for unit tests
* [bats](https://bats-core.readthedocs.io/en/stable/index.html) for run tests

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

You can also run tests with [Valgrind](http://www.valgrind.org) tool such as
`memcheck`:

```console
$ VALGRIND=memcheck meson test -C builddir
```

### Code coverage report

There is also possibility to generate code coverage report from test cases. To
do so you need [gcovr](https://gcovr.com/) and then you can run:

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
