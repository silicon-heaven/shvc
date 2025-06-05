# The support provided by Meson for running tests in valgrind consist of
# wrapping test executables with string provided by user. This has issue that
# not every time we run directly our testing code. In "run" tests we launch
# suite and we don't want to check that (it is too slow to do so). It also seems
# that it is not easilly possible to ignore initial process and only track
# children.
# The solution this script presents is to skip wrap for those tests that will
# wrap tested programs on their own.
import os
import os.path
import sys

valgrind = sys.argv[1]
valgrind_args = [
    "--quiet",
    "--error-exitcode=118",
    "--show-error-list=yes",
    f"--suppressions={os.path.abspath(os.path.dirname(__file__))}/valgrind.supp",
    "--track-fds=yes",
    "--trace-children=yes",
    "--child-silent-after-fork=no",
    "--memcheck:leak-check=full",
    "--memcheck:show-leak-kinds=definite,indirect,possible",
    "--memcheck:track-origins=yes",
    f"--tool={sys.argv[2]}",
]

if __name__ == "__main__":
    if os.getenv("NO_VALGRIND_WRAP"):
        os.environ["VALGRIND"] = valgrind
        os.environ["VALGRIND_OPTS"] = " ".join(valgrind_args)
        os.execvp(sys.argv[3], sys.argv[3:])
    else:
        os.execlp(valgrind, valgrind, *valgrind_args, "--", *sys.argv[3:])
