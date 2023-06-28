#!/usr/bin/env bats
set -eu

setup() {
	if [ -z "${BATS_TEST_TMPDIR:-}" ]; then
		BATS_TEST_TMPDIR="$(mktemp -d)"
		export BATS_TEST_TMPDIR
		export BATS_TEST_TMPDIR_CUSTOM="y"
	fi
}

teardown() {
	if [ "${BATS_TEST_TMPDIR_CUSTOM:-}" = "y" ]; then
		rm -rf "$BATS_TEST_TMPDIR"
	fi
}

foo() {
	local valgrind_log
	local ec=0
	valgrind_log="$(mktemp -p "$BATS_TEST_TMPDIR" "valgrind.XXXXXX")"
	${VALGRIND:-} ${VALGRIND:+--log-file="$valgrind_log" --} \
		"${TEST_FOO:-foo}" "$@" || ec=$?
	[ ! -s "$valgrind_log" ] || sed 's/^/#/' "$valgrind_log" >&3
	return $ec
}

@test "help" {
	skip "There is memory leak in GLibC when using --help"
	foo --help
}

@test "null" {
	[ "$(foo </dev/null)" == "0" ]
}

here_foo() {
	foo <<-EOF
		fee: ignored
		foo: counted once
		foo: coutned twice
		fee: again ignored
	EOF
}
@test "here" {
	[ "$(here_foo)" == "2" ]
}

@test "file" {
	file="$BATS_TEST_TMPDIR/file"
	cat >"$file" <<-EOF
		foo: once
		fee: none
		foo: two
		foo: three
	EOF
	[ "$(foo "$file")" == "3" ]
}

@test "missing" {
	local ec=0
	local error
	error="$(foo /dev/missing 2>&1)" || ec=$?
	[ "$ec" -eq 1 ]
	[ "$error" == "Can't open input file '/dev/missing': No such file or directory" ]
}
