#!/usr/bin/env bash
# This script performs two separate operations:
# * It creates the release commit and tag
# * It creates Gitlab release from Gitlab CI
set -eu
cd "${0%/*}"

# This should be set to the appropriate project name.
project_name="CHANGE THIS IN release.sh!"

fail() {
	echo "$@" >&2
	exit 1
}

in_ci() {
	[[ "${GITLAB_CI:-}" == "true" ]]
}

is_semver() {
	[[ "$1" =~ ^((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))(-((0|[1-9][0-9]*|[0-9]*[a-zA-Z-][0-9a-zA-Z-]*)(\.(0|[1-9][0-9]*|[0-9]*[a-zA-Z-][0-9a-zA-Z-]*))*))?(\+([0-9a-zA-Z-]+(\.[0-9a-zA-Z-]+)*))?$ ]]
}

releases() {
	sed -nE 's/^## \[([^]]+)\].*/\1/p' CHANGELOG.md
}

# Get the latest changelog.
latest_changelog() {
	awk '
		BEGIN { flag = 0; }
		/^## / && !flag { flag = 1; next; }
		/^## / && flag { exit; }
		flag { print; }
	' CHANGELOG.md
}

################################################################################

if ! in_ci; then

	changelog="$(latest_changelog | sed -E 's|^##[#]+ ||;s|^- |* |')"
	if [[ -z "$changelog" ]] || [[ "$(releases | head -1)" != "Unreleased" ]]; then
		fail "There is no unreleased changelog!"
	fi

	prev="$(releases | sed -n 2p)"
	while true; do
		read -rp "New release (previous: $prev): " version
		if ! is_semver "$version"; then
			echo "Version has to be valid semantic version!"
			continue
		fi
		if git rev-parse "v${version}" >/dev/null 2>&1; then
			echo "This version already exists."
			continue
		fi
		break
	done

	sed -i "0,/^## / s|## \\[Unreleased\\].*|## \\[${version}\\] - $(date +%Y-%m-%d)|" CHANGELOG.md
	[[ "$(releases | head -1)" == "$version" ]] || fail "Failed to set version in CHANGELOG.md"
	git commit -e -m "$project_name version $version" -m "$changelog" CHANGELOG.md

	while ! gitlint; do
		read -rp "Invalid commit message. Edit again? (Yes/no/force) " response
		case "${response,,}" in
		"" | y | yes)
			git commit --amend
			;;
		n | no)
			git reset --merge HEAD^
			exit 1
			;;
		f | force)
			break
			;;
		esac
	done

	git tag -s "v${version}" -m "$(git log --format=%B -n 1 HEAD)"

else

	[[ "$CI_COMMIT_TAG" =~ ^v ]] \
		|| fail "This is not a release tag: $CI_COMMIT_TAG"
	version="${CI_COMMIT_TAG#v}"
	is_semver "$version" \
		|| fail "Version has to be valid semantic version!"
	[[ "$(releases | head -1)" == "$version" ]] \
		|| fail "The version $version isn't the latest release in CHANGELOG.md "

	changelog="$(latest_changelog)"
	[[ -n "$changelog" ]] \
		|| fail "Changelog is empty!"

	declare -a args
	while read -r dists; do
		for dist in $dists; do
			[ -f "$dist" ] || continue # ignore missing / not built / directories
			ndist="${CI_PROJECT_NAME}-${dist%%.*}-${version}.${dist#*.}"
			spec="release/${version}/${ndist}"
			url="${CI_API_V4_URL}/projects/${CI_PROJECT_ID}/packages/generic/${spec}"
			echo "$dist -> $url"
			curl --header "JOB-TOKEN: ${CI_JOB_TOKEN}" \
				--upload-file "${dist}" "${url}"
			args+=("--assets-link" "{\"name\":\"${dist}\",\"url\":\"${url}\"}")
		done
	done < <(
		# shellcheck disable=SC2016
		yq '[. as $root | .release.needs[] | select(.artifacts) | $root[.job].artifacts.paths // $root[$root[.job].extends].artifacts.paths] | flatten | unique' .gitlab-ci.yml
	)

	release-cli create \
		--name "Release $version" \
		--tag-name "$CI_COMMIT_TAG" \
		--description "$changelog" \
		"${args[@]}"
fi
