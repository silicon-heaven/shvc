#!/usr/bin/env bats
set -eu
bats_require_minimum_version 1.11.0

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
	# FD magic: Bats uses FD 3 and 4 and with valgrind's --track-fds that causes
	# failure. Thus we close them for valgrind process. We also have to filter
	# out the valgrind output to allow easier matching and thus we swap stderr
	# with stdout, perform filtering and swap them around again.
	${VALGRIND:-} "${TEST_FOO:-foo}" "$@" 3>&1 2>&1 1>&3 3>&- 4>&- \
		| while read -r line; do
			if [[ "$line" =~ ^==.*==\ .* || "$line" =~ ^--.*--\ .* ]]; then
				echo "$line" >&4
			else
				echo "$line"
			fi
		done 3>&1 2>&1 1>&3
	return "${PIPESTATUS[0]}"
}

@test "help" {
	skip "There is memory leak in GLibC when using --help"
	foo --help
}

@test "null" {
	run -0 foo </dev/null
	[ "$output" = "0" ]
}

@test "here" {
	run -0 foo <<-EOF
		fee: ignored
		foo: counted once
		foo: coutned twice
		fee: again ignored
	EOF
	[ "$output" = "2" ]
}

@test "file" {
	file="$BATS_TEST_TMPDIR/file"
	cat >"$file" <<-EOF
		foo: once
		fee: none
		foo: two
		foo: three
	EOF
	run -0 foo "$file"
	[ "$output" = "3" ]
}

@test "missing" {
	run foo /dev/missing
	[ "$status" -eq 1 ]
	[ "$output" == "Can't open input file '/dev/missing': No such file or directory" ]
}
