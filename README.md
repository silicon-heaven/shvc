Silicon Heaven in C
===================

This provides implementation of Silicon Heaven communication protocol in C. This
implementation is based on top of threads where there is a dedicated thread that
manages the read while all threads can negotiate for write access.
