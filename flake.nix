{
  description = "Project template for C";

  inputs = {
    semver.url = "gitlab:Cynerd/nixsemver";
    check-suite.url = "github:cynerd/check-suite";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    semver,
    check-suite,
  }: let
    inherit (nixpkgs.lib) composeManyExtensions platforms;
    inherit (flake-utils.lib) eachDefaultSystem;
    inherit (semver.lib) changelog;

    version = changelog.currentRelease ./CHANGELOG.md self.sourceInfo;
    src = ./.;

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
            rev = self.rev or self.dirtyRev or null;
            hash = "sha256-OMlQ2CC1GNE/ZIJgNLu294X3mccCsLrwRRRUd26SDuI=";
          })
          gperf
          meson
          ninja
          pkg-config
          doxygen
          (sphinxHook.overrideAttrs {
            propagatedBuildInputs = with python3Packages; [
              sphinx-book-theme
              myst-parser
              breathe
            ];
          })
        ];
        postUnpack = "patchShebangs --build $sourceRoot";
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
        meta.mainProgram = "foo";
      };
  in
    {
      overlays = {
        pkgs = final: _: {
          template-c = final.callPackage template-c {};
        };
        default = composeManyExtensions [
          check-suite.overlays.default
          self.overlays.pkgs
        ];
      };
    }
    // eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
    in {
      packages.default = pkgs.template-c;
      legacyPackages = pkgs;

      devShells.default = pkgs.mkShell {
        packages = with pkgs; [
          # Linters and formatters
          clang-tools_18
          editorconfig-checker
          muon
          shellcheck
          shfmt
          statix
          deadnix
          gitlint
          # Testing and code coverage
          valgrind
          gcovr
          # Documentation
          sphinx-autobuild
        ];
        inputsFrom = [self.packages.${system}.default];
        meta.platforms = platforms.linux;
      };

      checks.default = self.packages.${system}.default;

      formatter = pkgs.alejandra;
    });
}
