"""Device used in the tests."""

import collections.abc
import typing

from shv import SHVType
from shv.rpcapi import SHVBase
from shv.rpcapi.client import SHVClient
from shv.rpcdef import (
    RpcAccess,
    RpcDir,
    RpcMethodCallExceptionError,
    RpcUserIDRequiredError,
)
from shv.rpcmessage import RpcMessage


class Device(SHVClient):
    def __init__(self, *args: typing.Any, **kwargs: typing.Any) -> None:  # noqa ANN401
        super().__init__(*args, **kwargs)
        self.value: SHVType = 42
        self.lock_owner: str | None = None

    def _ls(self, path: str) -> collections.abc.Iterator[str]:
        yield from super()._ls(path)
        if not path:
            yield "value"
            yield "lock"

    def _dir(self, path: str) -> collections.abc.Iterator[RpcDir]:
        yield from super()._dir(path)
        if path == "value":
            yield RpcDir.getter(result="Any", signal=True)
            yield RpcDir.setter(param="Any", description="Set value")
        if path == "lock":
            yield RpcDir("take", flags=RpcDir.Flag.USER_ID_REQUIRED)
            yield RpcDir("release", flags=RpcDir.Flag.USER_ID_REQUIRED)
            yield RpcDir.getter("owner", result="String|Null", signal=True)

    async def _method_call(self, request: SHVBase.Request) -> SHVType:
        match request.path, request.method:
            case ["value", "get"] if request.access >= RpcAccess.READ:
                return self.value
            case ["value", "set"] if request.access >= RpcAccess.WRITE:
                old = self.value
                self.value = request.param
                if old != request.param:
                    await self._send(RpcMessage.signal("value", value=request.param))
                return None
            case ["lock", "take"] if request.access >= RpcAccess.COMMAND:
                if request.user_id is None:
                    raise RpcUserIDRequiredError
                if self.lock_owner is not None:
                    raise RpcMethodCallExceptionError("Already taken")
                self.lock_owner = request.user_id
                await self._send(
                    RpcMessage.signal("lock", "owner", value=request.user_id)
                )
                return None
            case ["lock", "release"] if request.access >= RpcAccess.READ:
                if request.user_id is None:
                    raise RpcUserIDRequiredError
                if self.lock_owner != request.user_id:
                    raise RpcMethodCallExceptionError("Not taken by you")
                self.lock_owner = None
                await self._send(RpcMessage.signal("lock", "owner", value=None))
                return None
            case ["lock", "owner"] if request.access >= RpcAccess.READ:
                return self.lock_owner
        return await super()._method_call(request)
