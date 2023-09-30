{
  description = "Silicon Heaven in C";

  inputs = {
    check-suite.url = "github:cynerd/check-suite";
    pyshv.url = "git+https://gitlab.com/elektroline-predator/pyshv.git?ref=shv3";
  };

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    check-suite,
    pyshv,
  }:
    with builtins;
    with flake-utils.lib;
    with nixpkgs.lib; let
      version = fileContents ./version;
      src = builtins.path {
        path = ./.;
        filter = path: type: ! hasSuffix ".nix" path;
      };
      packages = pkgs: let
        subprojects = import ./subprojects/.fetch.nix {
          inherit pkgs src;
          rev = self.rev or null;
          hash = "sha256-GEUDy5crY09UTQ072NEvB50DjjgNv2BrLSwxLRNqEKM=";
        };
      in {
        shvc = pkgs.stdenv.mkDerivation {
          pname = "shvc";
          inherit version src;
          outputs = ["out" "doc"];
          buildInputs = with pkgs; [
              inih
              uriparser
              openssl
          ];
          nativeBuildInputs = with pkgs; [
            subprojects
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
          checkInputs = with pkgs; [
            check
            pkgs.check-suite
          ];
          nativeCheckInputs = with pkgs; [
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
      };
    in
      {
        overlays = {
          shvc = final: prev: packages (id prev);
          default = composeManyExtensions [
            check-suite.overlays.default
            pyshv.overlays.default
            self.overlays.shvc
          ];
        };
      }
      // eachDefaultSystem (system: let
        pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
      in {
        packages = {
          inherit (pkgs) shvc;
          default = pkgs.shvc;
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
