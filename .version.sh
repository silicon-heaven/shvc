#!/usr/bin/env bash
set -eu

version="$(cat "${0%/*}/version")"

replace_release() {
	echo "$version" | sed -E "s/^([^-+]*)-?[^+]*(.*)?$/\\1-$1\\2/"
	exit 0
}

if command -v git >/dev/null && git rev-parse --git-dir >/dev/null 2>&1; then
	if git diff-index --quiet HEAD; then
		git_hash="$(git rev-parse --short HEAD)"
		if [[ "$git_hash" != "$(git rev-parse --quiet --short "v$version^{commit}")" ]]; then
			replace_release "$git_hash"
		fi
	else
		replace_release "dirty"
	fi
fi
echo "$version"
