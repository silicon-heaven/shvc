{
  description = "Silicon Heaven in C";

  inputs = {
    semver.url = "gitlab:Cynerd/nixsemver";
    check-suite.url = "gitlab:cynerd/check-suite";
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
    forSystemsPkgs = func: forSystems (system: func system self.legacyPackages.${system});

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
      subprojects ? (callPackage ./subprojects {}),
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
          subprojects
          gperf
          meson
          ninja
          pkg-config
          (sphinxHook.overrideAttrs {
            propagatedBuildInputs = with python3Packages; [
              sphinx-book-theme
              sphinx-mdinclude
              hawkmoth
            ];
          })
          graphviz-nox
        ];
        postUnpack = "patchShebangs --build $sourceRoot";
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
        sphinxRoot = "../docs";
        meta.mainProgram = "shvc";
        passthru = {inherit subprojects;};
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

    packages = forSystemsPkgs (_: pkgs: {
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

    devShells = forSystemsPkgs (system: pkgs: {
      default = pkgs.mkShell {
        packages = with pkgs; [
          # Linters and formatters
          deadnix
          docstrfmt
          editorconfig-checker
          gitlint
          llvmPackages.clang-tools
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
          sphinx-lint
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
