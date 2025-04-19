{
  description = "Silicon Heaven in C";

  inputs = {
    semver.url = "gitlab:Cynerd/nixsemver";
    check-suite.url = "github:cynerd/check-suite";
    pyshv.url = "gitlab:silicon-heaven/pyshv";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    semver,
    check-suite,
    pyshv,
  }: let
    inherit (nixpkgs.lib) composeManyExtensions platforms;
    inherit (flake-utils.lib) eachDefaultSystem mkApp;
    inherit (semver.lib) changelog;

    version = changelog.currentRelease ./CHANGELOG.md self.sourceInfo;
    src = ./.;

    shvc = {
      stdenv,
      callPackage,
      gperf,
      meson,
      ninja,
      pkg-config,
      graphviz-nox,
      uriparser,
      openssl,
      check,
      check-suite,
      python3,
    }:
      stdenv.mkDerivation {
        pname = "shvc";
        inherit version src;
        GIT_REV = self.shortRev or self.dirtyShortRev;
        buildInputs = [
          uriparser
          openssl
        ];
        nativeBuildInputs = [
          (callPackage ./subprojects/.fetch.nix {
            inherit src;
            rev = self.rev or self.dirtyRev or null;
            hash = "sha256-QVxQpPqYAyOJiGDWLVsYTxYwOdwXOTbJxe05Tg8sJYo=";
          })
          gperf
          meson
          ninja
          pkg-config
          graphviz-nox
        ];
        postUnpack = "patchShebangs --build $sourceRoot";
        checkInputs = [
          check
          check-suite
        ];
        nativeCheckInputs = [
          (python3.withPackages (pypkgs:
            with pypkgs; [
              pytest
              pytest-tap
              pytest-asyncio
              pypkgs.pyshv
            ]))
        ];
        doCheck = true;
        meta.mainProgram = "shvc";
      };
  in
    {
      overlays = {
        pkgs = final: prev: {
          shvc = final.callPackage shvc {};
          pythonPackagesExtensions =
            prev.pythonPackagesExtensions
            ++ [self.overlays.pythonPackages];
        };
        pythonPackages = final: _: {
          hawkmoth = final.callPackage ./hawkmoth.nix {};
        };
        patchedSphinx = _: prev: {
          sphinx = prev.sphinx.overrideAttrs (old: {
            patches = (old.patches or []) ++ [./sphinx.patch];
          });
        };
        default = composeManyExtensions [
          check-suite.overlays.default
          pyshv.overlays.default
          self.overlays.pkgs
        ];
      };
    }
    // eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
    in {
      packages.default = pkgs.shvc;
      legacyPackages = pkgs;

      apps = {
        default = mkApp {
          drv = self.packages.${system}.default;
        };
        broker = mkApp {
          drv = self.packages.${system}.default;
          name = "shvbroker";
        };
      };

      devShells.default = pkgs.mkShell {
        packages = with pkgs; [
          # Linters and formatters
          clang-tools
          deadnix
          editorconfig-checker
          gitlint
          muon
          mypy
          ruff
          shellcheck
          shfmt
          statix
          # Testing and code coverage
          valgrind
          gcovr
          # Documentation
          clang
          ((python3.override {
              packageOverrides = self.overlays.patchedSphinx;
            }).withPackages (pypkgs:
              with pypkgs; [
                sphinx
                sphinx-autobuild
                sphinx-book-theme
                sphinx-mdinclude
                sphinx-multiversion
                hawkmoth
              ]))
        ];
        inputsFrom = [self.packages.${system}.default];
        meta.platforms = platforms.linux;
      };

      checks.default = self.packages.${system}.default;

      formatter = pkgs.alejandra;
    });
}
