#!/usr/bin/env bash
set -eu

version="${CI_COMMIT_TAG#v}"
if ! grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$' <<<"$version"; then
	echo "Tag has to be valid version number such as v1.0.0!" >&2
	exit 1
fi

if [[ "$(cat version)" != "$version" ]]; then
	echo "Version file does not contain the correct version!" >&2
	exit 1
fi

# Changelog should contain as a first section this release as this is the
# latest release.
changelog="$(awk -v "version=$version" '
		BEGIN {
			flag = 0
		}
		/^## / {
			if ( $0 !~ "^## \\[" version "\\]" ) {
				exit
			}
			flag = 1
			next
		}
		/^## \[/ && flag {
			exit
		}
		flag {
			print
		}
	' CHANGELOG.md)"
if [ -z "$changelog" ]; then
	echo "Changelog is empty." \
		"Have you updated the version? Is this the latest version?" >&2
	exit 1
fi

declare -a args
while read -r dists; do
	for dist in $dists; do
		[ -f "$dist" ] || continue # ignore missing / not built
		spec="release/${version}/${dist##*/}"
		url="${CI_API_V4_URL}/projects/${CI_PROJECT_ID}/packages/generic/${spec}"
		echo "$dist -> $url"
		curl --header "JOB-TOKEN: ${CI_JOB_TOKEN}" \
			--upload-file "${dist}" "${url}"
		args+=("--assets-link" "{\"name\":\"${dist}\",\"url\":\"${url}\"}")
	done
done <<<"$(yq '.build.artifacts.paths[]' .gitlab-ci.yml)"

release-cli create \
	--name "Release $version" \
	--tag-name "$CI_COMMIT_TAG" \
	--description "$changelog" \
	"${args[@]}"
