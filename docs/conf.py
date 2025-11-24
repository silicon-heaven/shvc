# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
import os
import pathlib

import docutils

project = "Silicon Heaven in C"
copyright = "SPDX-License-Identifier: MIT"
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
    "sphinx_mdinclude",
    "sphinx_multiversion",
    "hawkmoth",
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
    "repository_url": "https://gitlab.com/silicon-heaven/shvc",
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
hawkmoth_clang = [f"-I{hawkmoth_root}"]
hawkmoth_source_uri = (
    "https://gitlab.com/silicon-heaven/shvc/-/blob/master/include/{source}#L{line}"
)


def build_finished_gitignore(app, exception):
    """Create .gitignore file when build is finished."""
    outpath = pathlib.Path(app.outdir)
    if exception is None and outpath.is_dir():
        (outpath / ".gitignore").write_text("**\n")


man7url = "https://man7.org/linux/man-pages"
stdcmap = {
    "FILE": f"{man7url}/man3/FILE.3type.html",
    "int32_t": f"{man7url}/man3/intn_t.3type.html",
    "int64_t": f"{man7url}/man3/intn_t.3type.html",
    "intmax_t": f"{man7url}/man3/intn_t.3type.html",
    "obstack": "https://www.gnu.org/software/libc/manual/html_node/Obstacks.html",
    "pthread_attr_t": f"{man7url}/man0/sys_types.h.0p.html",
    "pthread_t": f"{man7url}/man0/sys_types.h.0p.html",
    "sig_atomic_t": f"{man7url}/man0/signal.h.0p.html",
    "size_t": f"{man7url}/man3/size_t.3type.html",
    "ssize_t": f"{man7url}/man3/size_t.3type.html",
    "timespec": f"{man7url}/man3/timespec.3type.html",
    "tm": f"{man7url}/man3/tm.3type.html",
    "uint32_t": f"{man7url}/man3/intn_t.3type.html",
    "uint64_t": f"{man7url}/man3/intn_t.3type.html",
    "uint8_t": f"{man7url}/man3/intn_t.3type.html",
    "uintmax_t": f"{man7url}/man3/intn_t.3type.html",
    "va_list": f"{man7url}/man3/va_list.3type.html",
    "strncpy": f"{man7url}/man3/strncpy.3p.html",
    "memcpy": f"{man7url}/man3/memcpy.3.html",
    "fscanf": f"{man7url}/man3/fscanf.3p.html",
}


def resolve_type_aliases(app, env, node, contnode):  # type: ignore
    """Resolve stdlibc references to man7.org."""
    target = node["reftarget"]
    if node["refdomain"] == "c" and target in stdcmap:
        return docutils.nodes.reference(
            "", target, refuri=stdcmap[target], classes=["external"]
        )


def setup(app):
    app.connect("build-finished", build_finished_gitignore)
    app.connect("missing-reference", resolve_type_aliases)
