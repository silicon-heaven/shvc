{
  pkgs,
  src,
  hash ? "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
  rev,
}: let
  pname =
    if rev != null
    then rev
    else "src";
in
  pkgs.makeSetupHook {
    name = "${pname}-meson-subprojects-setup-hook";

    substitutions = {
      subprojects = pkgs.stdenvNoCC.mkDerivation {
        name = "${pname}-meson-subprojects";
        inherit src;

        nativeBuildInputs = with pkgs; [meson cacert git];

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
  ./.nix-setup-hook.sh
