{
  description = "Project template for C";

  inputs = {
    check-suite.url = "github:cynerd/check-suite";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    check-suite,
  }:
    with builtins;
    with flake-utils.lib;
    with nixpkgs.lib; let
      version = fileContents ./version;
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
          outputs = ["out" "doc"];
          buildInputs = [];
          nativeBuildInputs = [
            (callPackage ./subprojects/.fetch.nix {
              inherit src;
              rev = self.rev or null;
              hash = "sha256-GEUDy5crY09UTQ072NEvB50DjjgNv2BrLSwxLRNqEKM=";
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
        defaultpkg = pkgs.template-c;
      in {
        packages = {
          inherit (pkgs) template-c;
          default = defaultpkg;
        };
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
              # Documentation
              sphinx-autobuild
            ];
            inputsFrom = [self.packages.${system}.template-c];
            meta.platforms = platforms.linux;
          };
        };

        apps.default = {
          type = "app";
          program = "${defaultpkg}/bin/foo";
        };

        checks.default = defaultpkg;

        formatter = pkgs.alejandra;
      });
}
