# shellcheck shell=bash

copySubprojects() {
	echo "Copying downloaded subprojects"
	# shellcheck disable=SC2154
	rm -rf "$sourceRoot/subprojects"
	cp -r --no-preserve=mode @subprojects@ "$sourceRoot/subprojects"
}

postUnpackHooks+=(copySubprojects)
