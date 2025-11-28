# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
import os
import pathlib
import docutils


project = "Elektroline C project Template"
copyright = "2023-2025, Elektroline a.s."
author = "Elektroline a.s."


extensions = [
    "sphinx.ext.doctest",
    "sphinx.ext.autodoc",
    "sphinx.ext.intersphinx",
    "sphinx.ext.viewcode",
    "sphinx.ext.todo",
    "sphinx_book_theme",
    "sphinx_mdinclude",
    "sphinx_multiversion",
    "hawkmoth",
]

templates_path = ["_templates"]
exclude_patterns = ["_build"]

# html_logo = "_static/logo.svg"
# html_favicon = "_static/favicon.ico"
html_copy_source = True
html_show_sourcelink = True
html_show_copyright = False
html_theme = "sphinx_book_theme"
# html_static_path = ["_static"]
html_sidebars = {
    "**": [
        "navbar-logo.html",
        "icon-links.html",
        "search-button-field.html",
        "sbt-sidebar-nav.html",
        "versioning.html",
    ]
}
html_theme_options = {
    "show_toc_level": 3,
    "repository_url": "https://gitlab.elektroline.cz/emb/template/python",
    "repository_branch": "master",
    "path_to_docs": "docs",
    "use_source_button": True,
    "use_repository_button": True,
    "use_edit_page_button": True,
    "use_issues_button": True,
}

smv_tag_whitelist = r"^v.*$"
smv_branch_whitelist = r"^master$"
smv_remote_whitelist = r"^.*$"


hawkmoth_root = os.path.abspath("../include")
hawkmoth_source_uri = "https://gitlab.elektroline.cz/emb/template/c/-/blob/master/include/{source}#L{line}"

# hawkmoth_clang: list[str] = []
# meson_dep_vars = {"obstack"}  # Meson dependency variable names
# with tempfile.TemporaryDirectory() as tdir:
#    subprocess.run(
#        ["meson", "setup", "--wipe", tdir, pathlib.Path(__file__).parents[1]],
#        check=True,
#    )
#    meson_introspect = subprocess.run(
#        ["meson", "introspect", "--dependencies", tdir],
#        stdout=subprocess.PIPE,
#        check=True,
#    ).stdout
#    for d in json.loads(meson_introspect):
#        if any(var in meson_dep_vars for var in d["meson_variables"]):
#            hawkmoth_clang.extend(f"-I{inc}" for inc in d["include_directories"])


def build_finished_gitignore(app, exception):
    """Create .gitignore file when build is finished."""
    outpath = pathlib.Path(app.outdir)
    if exception is None and outpath.is_dir():
        (outpath / ".gitignore").write_text("**\n")


stdc_types = [
    "FILE",
]


def resolve_type_aliases(app, env, node, contnode):  # type: ignore
    """Resolve stdlibc references to man7.org."""
    if node["refdomain"] == "c" and node["reftarget"] in stdc_types:
        url = f"https://man7.org/linux/man-pages/man3/{node['reftarget']}.3type.html"
        return docutils.nodes.reference(
            "", node["reftarget"], refuri=url, **({"classes": ["external"]})
        )


def setup(app):
    app.connect("build-finished", build_finished_gitignore)
    app.connect("missing-reference", resolve_type_aliases)
