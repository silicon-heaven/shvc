{
  description = "Silicon Heaven in C";

  inputs = {
    check-suite.url = "github:cynerd/check-suite";
    shvapp.url = "git+https://github.com/silicon-heaven/shvapp.git?submodules=1";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    check-suite,
    shvapp,
  }:
    with builtins;
    with flake-utils.lib;
    with nixpkgs.lib; let
      packages = pkgs:
        with pkgs; rec {
          shvc = stdenv.mkDerivation {
            pname = "shvc";
            version = replaceStrings ["\n"] [""] (readFile ./version);
            src = builtins.path {
              path = ./.;
              filter = path: type: ! hasSuffix ".nix" path;
            };
            outputs = ["out" "doc"];
            sphinxRoot = "../docs";
            buildInputs = [
              uriparser
            ];
            nativeBuildInputs = [
              # Build tools
              meson
              ninja
              gperf
              pkg-config
              # Documentation
              doxygen
              (sphinxHook.overrideAttrs (oldAttrs: {
                propagatedBuildInputs = with python3Packages; [
                  sphinx_rtd_theme
                  myst-parser
                  breathe
                ];
              }))
            ];
            doCheck = true;
            checkInputs = [
              # Unit tests
              check
              pkgs.check-suite
            ];
            nativeCheckInputs = [
              pkgs.shvapp
              # Run tests
              bash
              bats
            ];
          };
        };
    in
      {
        overlays = {
          shvc = final: prev: packages (id prev);
          default = composeManyExtensions [
            check-suite.overlays.default
            shvapp.overlays.default
            self.overlays.shvc
          ];
        };
      }
      // eachDefaultSystem (system: let
        pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
      in {
        packages = rec {
          inherit (pkgs) shvc;
          default = shvc;
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
            inputsFrom = [self.packages.${system}.shvc];
            meta.platforms = platforms.linux;
          };
        };

        checks.default = self.packages.${system}.shvc;

        formatter = pkgs.alejandra;
      });
}
