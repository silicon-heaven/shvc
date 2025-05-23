#!/usr/bin/env bash
# The support provided by Meson for running tests in valgrind consist of
# wrapping test executables with string provided by user. This has issue that
# not every time we run directly our testing code. In "run" tests we launch
# suite and we don't want to check that (it is too slow to do so). It also seems
# that it is not easilly possible to ignore initial process and only track
# children. It is also not possible to provide valgrind with path to the RC file
# and thus we would have to run it always from the same directory (to use
# ./.valgrindrc).
# The solution this script presents is to simply ignore wrap functionality and
# provide a way for user to enable valgrind wrapping in the correct level
# through environment variable.
set -eu
################################################################################
declare -a default_valgrind_args
default_valgrind_args+=('--quiet')
default_valgrind_args+=('--error-exitcode=118')
default_valgrind_args+=('--show-error-list=yes')
default_valgrind_args+=("--suppressions=$(readlink -f "${0%/*}")/valgrind.supp")
default_valgrind_args+=('--track-fds=yes')
default_valgrind_args+=('--trace-children=yes')
default_valgrind_args+=('--child-silent-after-fork=no')
default_valgrind_args+=('--trace-children-skip=*/pyshvbroker')

default_valgrind_args+=('--memcheck:leak-check=full')
default_valgrind_args+=('--memcheck:show-leak-kinds=definite,indirect,possible')
default_valgrind_args+=('--memcheck:track-origins=yes')
################################################################################

usage() {
	echo "Usage: $0 [OPTION].. BINARY [ARG].." >&2
}
help() {
	usage
	{
		echo "This script runs given binary either directly or trough valgrind"
		echo "depending on its configuration."
		echo
		echo "Options:"
		echo "  -h     Print this help text"
		echo "  -n     Do not wrap with valgrind but rather pass it as environment variable"
		echo "  -v BIN Valgrind binary to use"
		echo "  -a ARG Add given argument as valgrind argument"
		echo "Environment variables:"
		echo "  VALGRIND=TOOL: run tests with valgrind with given tool"
	} >&2
}

declare -a valgrind_args
valgrind="valgrind"
no_wrap="n"
while getopts "hna:" opt; do
	case "$opt" in
	h)
		help
		exit 0
		;;
	n)
		no_wrap="y"
		;;
	a)
		valgrind_args+=("$OPTARG")
		;;
	*)
		usage
		exit 1
		;;
	esac
done
shift $((OPTIND - 1))
[ "$#" -gt 0 ] || {
	usage
	exit 1
}

if [[ -v VALGRIND ]]; then
	if [[ "$no_wrap" == "y" ]]; then
		# TODO possibly use VALGRIND_OPTS instead
		export VALGRIND="$valgrind ${default_valgrind_args[*]} ${valgrind_args[*]} --tool=$VALGRIND"
		exec "$@"
	else
		exec "$valgrind" "${default_valgrind_args[@]}" "${valgrind_args[@]}" --tool="$VALGRIND" -- "$@"
	fi
else
	exec "$@"
fi
