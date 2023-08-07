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
      packages = pkgs: let
        subprojects = import ./subprojects/.fetch.nix {
          inherit pkgs src;
          rev = self.rev or null;
          hash = "sha256-GEUDy5crY09UTQ072NEvB50DjjgNv2BrLSwxLRNqEKM=";
        };
      in {
        template-c = pkgs.stdenv.mkDerivation {
          pname = "template-c";
          inherit version src;
          outputs = ["out" "doc"];
          buildInputs = with pkgs; [];
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
            bash
            bats
          ];
          doCheck = true;
          sphinxRoot = "../docs";
        };
      };
    in
      {
        overlays = {
          template-c = final: prev: packages (id prev);
          default = composeManyExtensions [
            check-suite.overlays.default
            self.overlays.template-c
          ];
        };
      }
      // eachDefaultSystem (system: let
        pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
      in {
        packages = {
          inherit (pkgs) template-c;
          default = pkgs.template-c;
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

        checks.default = self.packages.${system}.template-c;

        formatter = pkgs.alejandra;
      });
}
