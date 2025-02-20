"""Device used in the tests."""

import collections.abc
import typing

import shv


class Device(shv.SHVClient):
    def __init__(self, *args: typing.Any, **kwargs: typing.Any) -> None:  # noqa ANN401
        super().__init__(*args, **kwargs)
        self.value: shv.SHVType = 42
        self.lock_owner: str | None = None

    def _ls(self, path: str) -> collections.abc.Iterator[str]:
        yield from super()._ls(path)
        if not path:
            yield "value"
            yield "lock"

    def _dir(self, path: str) -> collections.abc.Iterator[shv.RpcMethodDesc]:
        yield from super()._dir(path)
        if path == "value":
            yield shv.RpcMethodDesc.getter(result="Any", signal=True)
            yield shv.RpcMethodDesc.setter(param="Any", description="Set value")
        if path == "lock":
            yield shv.RpcMethodDesc("take", flags=shv.RpcMethodFlags.USER_ID_REQUIRED)
            yield shv.RpcMethodDesc(
                "release", flags=shv.RpcMethodFlags.USER_ID_REQUIRED
            )
            yield shv.RpcMethodDesc.getter("owner", result="String|Null", signal=True)

    async def _method_call(self, request: shv.SHVBase.Request) -> shv.SHVType:
        match request.path, request.method:
            case ["value", "get"] if request.access >= shv.RpcMethodAccess.READ:
                return self.value
            case ["value", "set"] if request.access >= shv.RpcMethodAccess.WRITE:
                old = self.value
                self.value = request.param
                if old != request.param:
                    await self._send(
                        shv.RpcMessage.signal("value", value=request.param)
                    )
                return None
            case ["lock", "take"] if request.access >= shv.RpcMethodAccess.COMMAND:
                if request.user_id is None:
                    raise shv.RpcUserIDRequiredError
                if self.lock_owner is not None:
                    raise shv.RpcMethodCallExceptionError("Already taken")
                self.lock_owner = request.user_id
                await self._send(
                    shv.RpcMessage.signal("lock", "owner", value=request.user_id)
                )
                return None
            case ["lock", "release"] if request.access >= shv.RpcMethodAccess.READ:
                if request.user_id is None:
                    raise shv.RpcUserIDRequiredError
                if self.lock_owner != request.user_id:
                    raise shv.RpcMethodCallExceptionError("Not taken by you")
                self.lock_owner = None
                await self._send(shv.RpcMessage.signal("lock", "owner", value=None))
                return None
            case ["lock", "owner"] if request.access >= shv.RpcMethodAccess.READ:
                return self.lock_owner
        return await super()._method_call(request)
