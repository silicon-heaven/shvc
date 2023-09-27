# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path("..").absolute()))

project = "Silicon Heaven in C"
copyright = "2023, Elektroline a.s."
author = "Elektroline a.s."


extensions = [
    "sphinx.ext.doctest",
    "sphinx.ext.autodoc",
    "sphinx.ext.intersphinx",
    "sphinx.ext.viewcode",
    "sphinx.ext.todo",
    "sphinx_rtd_theme",
    "myst_parser",
    "breathe",
]

templates_path = ["_templates"]
exclude_patterns = ["_build"]

# html_logo = "_static/logo.svg"
# html_favicon = "_static/favicon.ico"
html_copy_source = True
html_show_sourcelink = True
html_show_copyright = False
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]


myst_enable_extensions = [
    "colon_fence",
]

includedir = pathlib.Path("../include")
files = [file.relative_to(includedir) for file in includedir.glob("**/*.h")]
breathe_projects_source = {"public_api": ("../include", files)}
breathe_default_project = "public_api"
breathe_doxygen_config_options = {
    "PREDEFINED": "__attribute__(...)= restrict=",
    "MACRO_EXPANSION": "YES",
    "EXPAND_ONLY_PREDEF": "YES",
    "EXTRACT_STATIC": "YES",
}


def build_finished_gitignore(app, exception):
    """Create .gitignore file when build is finished."""
    outpath = pathlib.Path(app.outdir)
    if exception is None and outpath.is_dir():
        (outpath / ".gitignore").write_text("**\n")


def setup(app):
    app.connect("build-finished", build_finished_gitignore)
