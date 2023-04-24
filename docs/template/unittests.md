Unit tests
==========

The template provides Check based unit testing. The default API of Check is
expanded using Check-Suite that moves most of the common code to the macros.

Before you continue reading you should study [Check's
API](https://libcheck.github.io/check/doc/doxygen/html/check_8h.html) and [Check
Suite's](https://gitlab.com/Cynerd/check-suite#user-content-usage) documentation

The organization of tests is that every C file is one specific suite. Every
suite contains one or more cases and cases contain one or more tests. The suite
creation is handled automatically as well as collection to central check handle.
You as a user have to define only test cases and tests.

:::{note}
The C file is not automatically compiled. You have to add it as source to
unit test application in the `tests/unit/meson.build` file.
:::
