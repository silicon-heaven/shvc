Meson subprojects
=================

Meson provides a convenient way to include subprojects withtout using git
submodules. The advantage is especially when we consider those subprojects
optional. Meson is able to decide if dependency is in the system and thus not
required or if it needs to be fetched and build with this project. In case of
git's submodules that would be on the user to decide.

The implementation is with Meson's wrap files. You need to create [wrap
file](https://mesonbuild.com/Wrap-dependency-system-manual.html) in
the `subprojects` directory. Make sure that you also add this file to
`subprojects/.gitignore` so it is included in your commit.

If you do not plan on using subprojects then you can remove the `subprojects`
directory. With that you also need to remove `subprojects` from `flake.nix`.
