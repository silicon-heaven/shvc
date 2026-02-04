{
  makeSetupHook,
  writeScript,
  fetchgit,
  lib,
}: let
  # Wraps to be fetched with their hashes ######################################
  # The following attribute set is recurise set of wraps that should be fetched
  # and their checksums.
  # The name of the wrap is the key and must match the wrap file name. It must
  # contain `hash` and optionally can contain `wraps` additional wraps.
  wraps = {
    uriparser.hash = "sha256-lgXMhDrPM/PsUnDO6LPgUEeXJgw6XL90HaRSZ3pJwNE=";
  };
  ##############################################################################

  fetchers = {
    "wrap-git" = {
      name,
      hash,
      # The following keys are from wrap file
      url,
      revision,
      clone-recursive ? false,
      ...
    }:
      fetchgit {
        inherit hash url;
        rev = revision;
        fetchSubmodules = clone-recursive;
        preFetch = "out=\"$out/${name}\"";
      };
  };

  fetchWraps = {
    dir ? ./.,
    wraps,
  }:
    lib.mapAttrsToList (
      name: v: let
        # TODO this could be replaced with generic INI loader
        file = dir + "/${name}.wrap";
        wrapLines = lib.splitString "\n" (lib.readFile file);
        type = lib.head (builtins.match "\\[(.+)]" (lib.head wrapLines));
        attrs = lib.listToAttrs (
          lib.remove null
          (map (line: let
            match = builtins.match "([[:graph:]]+)[[:space:]]*=[[:space:]]*([[:graph:]]+)[[:space:]]*" line;
          in
            if match == null
            then (builtins.traceVerbose "Ignoring Wrap line: ${file}: ${line}" null)
            else lib.nameValuePair (lib.elemAt match 0) (lib.elemAt match 1))
          (lib.tail wrapLines))
        );
        fetcher = fetchers."${type}" or (lib.abort "Unsupported wrap: ${type}");
        repo = fetcher (attrs
          // {
            inherit name;
            inherit (v) hash;
          });
      in
        [repo]
        ++ (fetchWraps {
          dir = repo + "/${name}/subprojects";
          wraps = v.wraps or {};
        })
    )
    wraps;
in
  makeSetupHook {
    name = "meson-subprojects-setup-hook";
    substitutions.subprojects = fetchWraps {inherit wraps;};
  } (
    writeScript "meson-subprojects-setup-hook-script.sh" ''
      copySubprojects() {
        echo "Copying downloaded subprojects"
        for subproject in @subprojects@; do
          cp -r --no-preserve=mode "$subproject"/* "$sourceRoot/subprojects/"
        done
      }

      postUnpackHooks+=(copySubprojects)
    ''
  )
