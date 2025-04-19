RPC Method Call
===============

.. code-block:: c

    #include <shv/rpccall.h>

.. c:autodoc:: shv/rpccall.h

Stages diagram
--------------

.. graphviz::

    digraph rpccall {
      entrypoint [shape=invhouse]
      return [shape=house]
      entrypoint -> CALL_S_REQUEST
      CALL_S_REQUEST -> {CALL_S_REQUEST; CALL_S_COMERR; CALL_S_TIMERR}
      CALL_S_REQUEST -> {CALL_S_RESULT; CALL_S_DONE}
      CALL_S_RESULT -> {CALL_S_REQUEST; CALL_S_DONE}
      {CALL_S_DONE; CALL_S_TIMERR; CALL_S_COMERR} -> return
    }
