{
  description = "Project template for C";

  inputs = {
    semver.url = "gitlab:cynerd/nixsemver";
    check-suite.url = "github:cynerd/check-suite";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    semver,
    check-suite,
  }: let
    inherit (nixpkgs.lib) hasSuffix composeManyExtensions platforms;
    inherit (flake-utils.lib) eachDefaultSystem filterPackages mkApp;
    inherit (semver.lib) changelog;

    version = changelog.currentRelease ./CHANGELOG.md self.sourceInfo;
    src = builtins.path {
      path = ./.;
      filter = path: type: ! hasSuffix ".nix" path;
    };

    template-c = {
      stdenv,
      callPackage,
      gperf,
      meson,
      ninja,
      pkg-config,
      doxygen,
      sphinxHook,
      python3Packages,
      check,
      check-suite,
      bats,
      bash,
    }:
      stdenv.mkDerivation {
        pname = "template-c";
        inherit version src;
        GIT_REV = self.shortRev or self.dirtyShortRev;
        outputs = ["out" "doc"];
        buildInputs = [];
        nativeBuildInputs = [
          (callPackage ./subprojects/.fetch.nix {
            inherit src;
            rev = self.rev or null;
            hash = "sha256-Q0pRpk/cNHw7SzJ/AfxQuNDlAMuIRjuh9+vTwZfnXf4=";
          })
          gperf
          meson
          ninja
          pkg-config
          doxygen
          (sphinxHook.overrideAttrs (oldAttrs: {
            propagatedBuildInputs = with python3Packages; [
              sphinx_rtd_theme
              myst-parser
              breathe
            ];
          }))
        ];
        checkInputs = [
          check
          check-suite
        ];
        nativeCheckInputs = [
          bash
          bats
        ];
        doCheck = true;
        sphinxRoot = "../docs";
      };
  in
    {
      overlays = {
        noInherit = final: prev: {
          template-c = final.callPackage template-c {};
        };
        default = composeManyExtensions [
          check-suite.overlays.default
          self.overlays.noInherit
        ];
      };
    }
    // eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
    in {
      packages.default = pkgs.template-c;
      legacyPackages = pkgs;

      devShells = filterPackages system {
        default = pkgs.mkShell {
          packages = with pkgs; [
            # Linters and formaters
            clang-tools_14
            cppcheck
            editorconfig-checker
            flawfinder
            muon
            shellcheck
            shfmt
            gitlint
            # Testing and code coverage
            valgrind
            gcovr
            gcc11 # Hotfix for https://github.com/NixOS/nixpkgs/pull/279455
            # Documentation
            sphinx-autobuild
          ];
          inputsFrom = [self.packages.${system}.default];
          meta.platforms = platforms.linux;
        };
      };

      apps.default = mkApp {
        drv = self.packages.${system}.default;
        name = "foo";
      };

      checks.default = self.packages.${system}.default;

      formatter = pkgs.alejandra;
    });
}
