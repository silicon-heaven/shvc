{
  description = "Project template for C";

  inputs = {
    check-suite.url = "github:cynerd/check-suite/v1.0.1";
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
      packages = {
        system,
        pkgs ? nixpkgs.legacyPackages.${system},
      }: rec {
        template-c = with pkgs;
          stdenv.mkDerivation {
            pname = "template-c";
            version = readFile ./version;

            src = ./.;

            buildInputs = [
              check
              check-suite.packages.${system}.check-suite
            ];
            nativeBuildInputs = [
              meson
              ninja
              pkg-config
              gperf
              bash
              bats
            ];

            doCheck = true;
          };
        default = template-c;
      };
    in
      {
        overlays.default = final: prev:
          packages {
            inherit (prev) system;
            pkgs = prev;
          };
      }
      // eachDefaultSystem (system: {
        packages = packages {inherit system;};

        legacyPackages = import nixpkgs {
          # The legacyPackages imported as overlay allows us to use pkgsCross
          inherit system;
          overlays = [self.overlays.default];
          crossOverlays = [self.overlays.default];
        };

        devShells = filterPackages system {
          default = nixpkgs.legacyPackages.${system}.mkShell {
            packages = with nixpkgs.legacyPackages.${system}; [
              cppcheck
              flawfinder
              clang-tools_14
              shellcheck
              muon
              shfmt
              editorconfig-checker
              valgrind
              lcov
              gcovr
            ];
            inputsFrom = [self.packages.${system}.template-c];
            meta.platforms = platforms.linux;
          };
        };

        checks.default = self.packages.${system}.template-c;

        formatter = nixpkgs.legacyPackages.${system}.alejandra;
      });
}
