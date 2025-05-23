{
  makeSetupHook,
  writeScript,
  stdenvNoCC,
  meson,
  cacert,
  git,
  # The following should be specified by caller
  src,
  hash ? "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
  rev,
}: let
  pname =
    if rev != null
    then rev
    else "src";
in
  makeSetupHook {
    name = "${pname}-meson-subprojects-setup-hook";

    substitutions = {
      subprojects = stdenvNoCC.mkDerivation {
        name = "${pname}-meson-subprojects";
        inherit src;

        nativeBuildInputs = [meson cacert git];

        buildCommand = ''
          cp -r --no-preserve=mode $src/. .
          content=""
          while [ "$content" != "$(ls subprojects)" ]; do
            content="$(ls subprojects)"
            meson subprojects download || true
            find subprojects -mindepth 2 -name \*.wrap \
              | while read -r path; do
                file="''${path##*/}"
                [ -f "subprojects/$file" ] || {
                  echo '[wrap-redirect]'
                  echo "filename = ''${path#subprojects/}"
                } >"subprojects/$file"
              done
          done
          find subprojects -type d -name .git -prune -execdir rm -r {} +

          mv subprojects $out
        '';

        outputHashMode = "recursive";
        outputHash = hash;
      };
    };
  }
  (writeScript "run-hello-hook.sh" ''
    copySubprojects() {
      echo "Copying downloaded subprojects"
      # shellcheck disable=SC2154
      rm -rf "$sourceRoot/subprojects"
      cp -r --no-preserve=mode @subprojects@ "$sourceRoot/subprojects"
    }

    postUnpackHooks+=(copySubprojects)
  '')
