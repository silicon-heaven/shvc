Documentation
=============

The documentation must be written in rST. There are multiple sources for rST
cheat sheets. Unfortunatelly none of the is absolutelly perfect and thus feel
free to use any of these:

* https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html
* https://sphinx-tutorial.readthedocs.io/cheatsheet/
* https://bashtage.github.io/sphinx-material/rst-cheatsheet/rst-cheatsheet.html


Documenting in the code
-----------------------

API documentation should be in the header files in Doxygen format. The prefered
formating is:

.. code-block:: c

   /** Count number of foo lines in data read from provided file.
    *
    * :param f: file object open for reading
    * :return: number of "foo:" prefix lines found in file.
    */
   unsigned count_foo(FILE *f) __attribute__((nonnull));

Notice that we start comment with ``/**`` and the way return and parameters are
documented.
