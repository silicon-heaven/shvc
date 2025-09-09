{
  description = "Silicon Heaven in C";

  inputs = {
    semver.url = "gitlab:Cynerd/nixsemver";
    check-suite.url = "github:cynerd/check-suite";
    pyshv.url = "gitlab:silicon-heaven/pyshv";
  };

  outputs = {
    self,
    systems,
    nixpkgs,
    semver,
    check-suite,
    pyshv,
  }: let
    inherit (nixpkgs.lib) genAttrs composeManyExtensions;
    inherit (semver.lib) changelog;
    forSystems = genAttrs (import systems);
    withPkgs = func: forSystems (system: func self.legacyPackages.${system});

    name = "shvc";
    version = changelog.currentRelease ./CHANGELOG.md self.sourceInfo;
    src = ./.;

    package = {
      stdenv,
      callPackage,
      gperf,
      meson,
      ninja,
      pkg-config,
      uriparser,
      openssl,
      sphinxHook,
      python3Packages,
      graphviz-nox,
      check,
      check-suite,
      python3,
    }:
      stdenv.mkDerivation {
        pname = name;
        inherit version src;
        GIT_REV = self.shortRev or self.dirtyShortRev;
        outputs = ["out" "doc"];
        buildInputs = [
          uriparser
          openssl
        ];
        nativeBuildInputs = [
          (callPackage ./subprojects/.fetch.nix {
            inherit src;
            rev = self.rev or self.dirtyRev or null;
            hash = "sha256-OGVJ5KqNBmaW1cMARy2tI/ySeq0sesjbuBCYGdAzdro=";
          })
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
          graphviz-nox
        ];
        postUnpack = "patchShebangs --build $sourceRoot";
        sphinxRoot = "../docs";
        checkInputs = [
          check
          check-suite
        ];
        nativeCheckInputs = [
          (python3.withPackages (pypkgs:
            with pypkgs;
              [
                pytest
                pytest-tap
                pytest-asyncio
                pypkgs.pyshv
              ]
              ++ pypkgs.pyshv.optional-dependencies.canbus))
        ];
        doCheck = true;
        meta.mainProgram = "shvc";
      };
  in {
    overlays = {
      pkgs = final: _: {
        "${name}" = final.callPackage package {};
      };
      default = composeManyExtensions [
        check-suite.overlays.default
        pyshv.overlays.default
        self.overlays.pkgs
      ];
    };

    packages = withPkgs (pkgs: {
      default = pkgs."${name}";
    });

    apps = forSystems (system: {
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/shvc";
      };
      broker = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/shvbroker";
      };
    });

    devShells = withPkgs (pkgs: {
      default = pkgs.mkShell {
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
          sphinx-autobuild
        ];
        inputsFrom = [
          self.packages.${pkgs.hostPlatform.system}.default
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
