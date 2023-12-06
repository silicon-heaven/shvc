#!/usr/bin/env bash
set -eu

if command -v git >/dev/null && git rev-parse --git-dir >/dev/null 2>&1; then
	GIT_REV="$(git rev-parse --quiet --short HEAD)"
	git diff-index --quiet HEAD \
		|| GIT_REV+="-dirty"
fi

awk -v rev="${GIT_REV:-}" '
	BEGIN { flag = 0; }
	match($0, /^## \[([^]]*)]/, arr) {
		if (arr[1] == "Unreleased") {
			if (flag) { exit 1; }
			flag = 1;
		} else {
			if (flag || rev ~ /-dirty$/) {
				match(arr[1], /^((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))(-((0|[1-9][0-9]*|[0-9]*[a-zA-Z-][0-9a-zA-Z-]*)(\.(?:0|[1-9][0-9]*|[0-9]*[a-zA-Z-][0-9a-zA-Z-]*))*))?(\+([0-9a-zA-Z-]+(\.[0-9a-zA-Z-]+)*))?$/, v)
				if (v[5]) {
					print v[1] v[5] "." rev v[10]
				} else {
					print v[1] "-" rev v[10]
				}
			} else {
				print arr[1]
			}
			exit
		}
	}
' "${0%/*}/CHANGELOG.md" - <<<"## [0.0.0]"
