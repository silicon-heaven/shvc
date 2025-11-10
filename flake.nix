{
  description = "Project template for C";

  inputs = {
    semver.url = "gitlab:Cynerd/nixsemver";
    check-suite.url = "github:cynerd/check-suite";
  };

  outputs = {
    self,
    systems,
    nixpkgs,
    semver,
    check-suite,
  }: let
    inherit (nixpkgs.lib) genAttrs composeManyExtensions;
    inherit (semver.lib) changelog;
    forSystems = genAttrs (import systems);
    withPkgs = func: forSystems (system: func self.legacyPackages.${system});

    name = "template";
    version = changelog.currentRelease ./CHANGELOG.md self.sourceInfo;
    src = ./.;

    package = {
      stdenv,
      callPackage,
      gperf,
      meson,
      ninja,
      pkg-config,
      sphinxHook,
      python3Packages,
      check,
      check-suite,
      bats,
      bash,
    }:
      stdenv.mkDerivation {
        pname = name;
        inherit version src;
        GIT_REV = self.shortRev or self.dirtyShortRev;
        outputs = ["out" "doc"];
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
          (sphinxHook.overrideAttrs {
            propagatedBuildInputs = with python3Packages; [
              sphinx-book-theme
              myst-parser
              hawkmoth
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
  in {
    overlays = {
      pkgs = final: _: {
        "${name}" = final.callPackage package {};
      };
      default = composeManyExtensions [
        check-suite.overlays.default
        self.overlays.pkgs
      ];
    };

    packages = withPkgs (pkgs: {
      default = pkgs."${name}";
    });

    devShells = withPkgs (pkgs: {
      default = pkgs.mkShell {
        packages = with pkgs; [
          # Linters and formatters
          llvmPackages.clang-tools
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
        inputsFrom = [
          self.packages.${pkgs.stdenv.hostPlatform.system}.default
        ];
      };
    });

    checks = forSystems (system: {
      inherit (self.packages.${system}) default;
    });
    formatter = withPkgs (pkgs: pkgs.alejandra);

    legacyPackages =
      forSystems (system:
        nixpkgs.legacyPackages.${system}.extend self.overlays.default);
  };
}
