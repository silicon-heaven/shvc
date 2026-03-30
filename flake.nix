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
    forSystemsPkgs = func: forSystems (system: func system self.legacyPackages.${system});

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
      subprojects ? (callPackage ./subprojects {}),
    }:
      stdenv.mkDerivation {
        pname = name;
        inherit version src;
        GIT_REV = self.shortRev or self.dirtyShortRev;
        outputs = ["out" "doc"];
        nativeBuildInputs = [
          subprojects
          gperf
          meson
          ninja
          pkg-config
          (sphinxHook.overrideAttrs {
            propagatedBuildInputs = with python3Packages; [
              sphinx-book-theme
              sphinx-mdinclude
              sphinx-multiversion
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
        passthru = {inherit subprojects;};
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

    packages = forSystemsPkgs (_: pkgs: {
      default = pkgs."${name}";
      inherit (pkgs."${name}") subprojects; # for an easier hash update
    });

    devShells = forSystemsPkgs (system: pkgs: {
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
          # Release
          yq
        ];
        inputsFrom = [
          (self.packages.${system}.default.override {
            # No need to build this for development shell
            subprojects = pkgs.emptyFile;
          })
        ];
      };
    });

    checks = forSystems (system: {
      inherit (self.packages.${system}) default;
    });
    formatter = forSystemsPkgs (_: pkgs: pkgs.alejandra);

    legacyPackages =
      forSystems (system:
        nixpkgs.legacyPackages.${system}.extend self.overlays.default);
  };
}
