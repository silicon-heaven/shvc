# shellcheck shell=bash
# The support provided by Meson for running tests in valgrind consist of
# wrapping test executables with string provided by user. This has issue that
# not every time we run directly our testing code. In "run" tests we launch
# suite and we don't want to check that (it is too slow to do so). It also seems
# that it is not easilly possible to ignore initial process and only track
# children.
# The solution this script presents is to skip wrap for those tests that will
# wrap tested programs on their own.
set -eu
################################################################################
declare -a valgrind_args
valgrind_args+=('--quiet')
valgrind_args+=('--error-exitcode=118')
valgrind_args+=('--show-error-list=yes')
valgrind_args+=("--suppressions=$(readlink -f "${0%/*}")/valgrind.supp")
valgrind_args+=('--track-fds=yes')
valgrind_args+=('--trace-children=yes')
valgrind_args+=('--child-silent-after-fork=no')

valgrind_args+=('--memcheck:leak-check=full')
valgrind_args+=('--memcheck:show-leak-kinds=definite,indirect,possible')
valgrind_args+=('--memcheck:track-origins=yes')
################################################################################

valgrind="$1"
tool="$2"
shift 2

if [[ -n "${NO_VALGRIND_WRAP:-}" ]]; then
	export VALGRIND="$valgrind"
	# TODO include passed VALGRIND_OPTS but before or after?
	export VALGRIND_OPTS="${valgrind_args[*]} --tool=$tool"
	exec "$@"
else
	exec "$valgrind" "${valgrind_args[@]}" --tool="$tool" -- "$@"
fi
