#!/usr/bin/env bash
set -eu

if command -v git >/dev/null && git rev-parse --git-dir >/dev/null 2>&1; then
	GIT_REV="$(git rev-parse --quiet --short HEAD)"
	git diff-index --quiet HEAD \
		|| GIT_REV+="-dirty"
fi

version=0.0.0
label=
build=
unreleased=n

while read -r line; do
	[[ "$line" =~ \#\#\ \[([^]]*)\].* ]] || continue
	if [[ "${BASH_REMATCH[1]}" == "Unreleased" ]]; then
		unreleased=y
	else
		if [[ "${BASH_REMATCH[1]}" =~ ^((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))(-((0|[1-9][0-9]*|[0-9]*[a-zA-Z-][0-9a-zA-Z-]*)(\.(0|[1-9][0-9]*|[0-9]*[a-zA-Z-][0-9a-zA-Z-]*))*))?(\+([0-9a-zA-Z-]+(\.[0-9a-zA-Z-]+)*))?$ ]]; then
			version="${BASH_REMATCH[1]}"
			label="${BASH_REMATCH[6]}"
			build="${BASH_REMATCH[11]}"
		fi
		break
	fi
done <"${0%/*}/CHANGELOG.md"

if [[ "$unreleased" == y ]] || [[ "$GIT_REV" =~ -dirty$ ]]; then
	label="${label}${label:+.}${GIT_REV}"
fi
echo "$version${label:+-}${label}${build:++}${build}"
