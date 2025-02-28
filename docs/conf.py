# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
import os
import pathlib
import sys
import docutils

sys.path.insert(0, str(pathlib.Path("..").absolute()))

project = "Elektroline C project Template"
copyright = "2023, Elektroline a.s."
author = "Elektroline a.s."


extensions = [
    "sphinx.ext.doctest",
    "sphinx.ext.autodoc",
    "sphinx.ext.intersphinx",
    "sphinx.ext.viewcode",
    "sphinx.ext.todo",
    "sphinx_book_theme",
    "myst_parser",
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


myst_enable_extensions = [
    "colon_fence",
]

hawkmoth_root = os.path.abspath("../include")
hawkmoth_source_uri = "https://gitlab.elektroline.cz/emb/template/c/-/blob/master/include/{source}#L{line}"


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
