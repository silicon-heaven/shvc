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
primary_domain = "c"
highlight_language = "c"


extensions = [
    "sphinx.ext.doctest",
    "sphinx.ext.autodoc",
    "sphinx.ext.intersphinx",
    "sphinx.ext.viewcode",
    "sphinx.ext.todo",
    "sphinx.ext.graphviz",
    "sphinx_book_theme",
    "myst_parser",
    "breathe",
]

templates_path = ["_templates"]
exclude_patterns = ["_build"]

html_logo = "_static/logo.svg"
html_favicon = "_static/favicon.ico"
html_copy_source = True
html_show_sourcelink = True
html_show_copyright = False
html_theme = "sphinx_book_theme"
html_static_path = ["_static"]


myst_enable_extensions = [
    "colon_fence",
]

nitpick_ignore = [
    ("c:identifier", v)
    for v in (
        # C (no standard sphinx documentation to reference)
        "uint8_t",
        "uint32_t",
        "uint64_t",
        "uintmax_t",
        "int32_t",
        "int64_t",
        "intmax_t",
        "size_t",
        "ssize_t",
        "va_list",
        "tm",
        "timespec",
        "FILE",
        "sig_atomic_t",
        "pthread_t",
        "pthread_attr_t",
        "obstack",
        # OpenSSL (no sphinx documentation to reference)
        "sha1ctx",
        # Internal structures
        "rpcbroker",
        "rpchandler",
        "rpchandler_app",
        "rpchandler_device",
        "rpchandler_responses",
        "rpcresponse",
        "rpchandler_signals",
        "rpchandler_login",
        "rpchandler_history",
        "rpchandler_file",
        "rpclogger",
        "rpchandler_device_alerts",
    )
]

includedir = pathlib.Path("../include")
files = [file.relative_to(includedir) for file in includedir.glob("**/*.h")]
breathe_projects_source = {"public_api": ("../include", files)}
breathe_default_project = "public_api"
breathe_domain_by_extension = {"h": "c"}
breathe_doxygen_config_options = {
    "PREDEFINED": "restrict=",
    "MACRO_EXPANSION": "YES",
    "EXPAND_ONLY_PREDEF": "YES",
    "EXTRACT_STATIC": "YES",
}

smv_tag_whitelist = r"^v.*$"
smv_branch_whitelist = r"^master$"
smv_remote_whitelist = r"^.*$"


def build_finished_gitignore(app, exception):
    """Create .gitignore file when build is finished."""
    outpath = pathlib.Path(app.outdir)
    if exception is None and outpath.is_dir():
        (outpath / ".gitignore").write_text("**\n")


def setup(app):
    app.connect("build-finished", build_finished_gitignore)
