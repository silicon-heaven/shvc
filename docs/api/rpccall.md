# RPC Method Call

```c
#include <shv/rpccall.h>
```

```{autodoxygenfile} shv/rpccall.h
```

## Stages diagram

```{graphviz}
digraph rpccall {
  entrypoint -> CALL_S_PACK
  CALL_S_PACK -> {CALL_S_PACK; CALL_S_COMERR; return}
  CALL_S_PACK -> {CALL_S_RESULT; CALL_S_VOID_RESULT; CALL_S_ERROR}
  {CALL_S_RESULT; CALL_S_VOID_RESULT; CALL_S_ERROR} -> {CALL_S_PACK, CALL_S_TIMERR, return}
  {CALL_S_TIMERR, CALL_S_COMERR} -> return
}
```
