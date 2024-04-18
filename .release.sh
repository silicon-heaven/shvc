#!/usr/bin/env bash
set -eu

version="${CI_COMMIT_TAG#v}"
if ! pysemver check "$version"; then
	echo "Tag has to be valid semantic version!" >&2
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
		[ -f "$dist" ] || continue # ignore missing / not built / directories
		spec="release/${version}/${dist##*/}"
		url="${CI_API_V4_URL}/projects/${CI_PROJECT_ID}/packages/generic/${spec}"
		echo "$dist -> $url"
		curl --header "JOB-TOKEN: ${CI_JOB_TOKEN}" \
			--upload-file "${dist}" "${url}"
		args+=("--assets-link" "{\"name\":\"${dist}\",\"url\":\"${url}\"}")
	done
done < <(
	# shellcheck disable=SC2016
	yq '[. as $root | .release.needs[] | select(.artifacts) | $root[.job].artifacts.paths] | flatten' .gitlab-ci.yml
)

release-cli create \
	--name "Release $version" \
	--tag-name "$CI_COMMIT_TAG" \
	--description "$changelog" \
	"${args[@]}"
