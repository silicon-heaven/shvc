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
              bash
              bats
              gperf
              meson
              ninja
              pkg-config
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
      // eachDefaultSystem (system: let
        pkgs = nixpkgs.legacyPackages.${system};
        pkgsSelf = self.packages.${system};
      in {
        packages = packages {inherit system;};
        legacyPackages = pkgs.extend self.overlays.default;

        devShells = filterPackages system {
          default = pkgs.mkShell {
            packages = with pkgs; [
              clang-tools_14
              cppcheck
              editorconfig-checker
              flawfinder
              gcovr
              gitlint
              muon
              shellcheck
              shfmt
              valgrind
            ];
            inputsFrom = [pkgsSelf.template-c];
            meta.platforms = platforms.linux;
          };
        };

        checks.default = self.packages.${system}.template-c;

        formatter = pkgs.alejandra;
      });
}
