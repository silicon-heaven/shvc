{
  description = "Silicon Heaven in C";

  inputs = {
    semver.url = "gitlab:cynerd/nixsemver";
    check-suite.url = "github:cynerd/check-suite";
    pyshv.url = "git+https://gitlab.com/silicon-heaven/pyshv.git";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    semver,
    check-suite,
    pyshv,
  }: let
    inherit (nixpkgs.lib) hasSuffix composeManyExtensions platforms;
    inherit (flake-utils.lib) eachDefaultSystem filterPackages mkApp;
    inherit (semver.lib) changelog;

    version = changelog.currentRelease ./CHANGELOG.md self.sourceInfo;
    src = builtins.path {
      path = ./.;
      filter = path: type: ! hasSuffix ".nix" path;
    };

    shvc = {
      stdenv,
      callPackage,
      gperf,
      meson,
      ninja,
      pkg-config,
      doxygen,
      sphinxHook,
      python3Packages,
      inih,
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
        outputs = ["out" "doc"];
        buildInputs = [
          inih
          uriparser
          openssl
        ];
        nativeBuildInputs = [
          (callPackage ./subprojects/fetch.nix {
            inherit src;
            rev = self.rev or null;
            hash = "sha256-4nmUv845q8NQ8LkQjwiltfsr2/+GYwdv37Yz0YTCMJE=";
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
          (python3.withPackages (pypkgs:
            with pypkgs; [
              pytest
              pytest-tap
              pypkgs.pyshv
            ]))
        ];
        doCheck = true;
        sphinxRoot = "../docs";
      };

    pypkg-multiversion = {
      buildPythonPackage,
      fetchFromGitHub,
      sphinx,
    }:
      buildPythonPackage {
        pname = "sphinx-multiversion";
        version = "0.2.4";
        src = fetchFromGitHub {
          owner = "Holzhaus";
          repo = "sphinx-multiversion";
          rev = "v0.2.4";
          hash = "sha256-ZFEELAeZ/m1pap1DmS4PogL3eZ3VuhTdmwDOg5rKOPA=";
        };
        propagatedBuildInputs = [sphinx];
        doCheck = false;
      };
  in
    {
      overlays = {
        pythonPackagesExtension = final: prev: {
          sphinx-multiversion = final.callPackage pypkg-multiversion {};
        };
        noInherit = final: prev: {
          pythonPackagesExtensions = prev.pythonPackagesExtensions ++ [self.overlays.pythonPackagesExtension];
          shvc = final.callPackage shvc {};
        };
        default = composeManyExtensions [
          check-suite.overlays.default
          pyshv.overlays.default
          self.overlays.noInherit
        ];
      };
    }
    // eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
    in {
      packages.default = pkgs.shvc;
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
            python3Packages.sphinx-multiversion
          ];
          inputsFrom = [self.packages.${system}.default];
          meta.platforms = platforms.linux;
        };
      };

      checks.default = self.packages.${system}.default;

      formatter = pkgs.alejandra;
    });
}
