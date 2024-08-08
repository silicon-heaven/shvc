"""Device used in the tests."""

import shv
import typing
import collections.abc


class Device(shv.SimpleClient):
    def __init__(self, *args: typing.Any, **kwargs: typing.Any) -> None:  # noqa ANN401
        super().__init__(*args, **kwargs)
        self.value: shv.SHVType = 42
        self.lock_owner: str | None = None

    def _ls(self, path: str) -> collections.abc.Iterator[str]:
        yield from super()._ls(path)
        if path == "":
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

    async def _method_call(
        self,
        path: str,
        method: str,
        param: shv.SHVType,
        access: shv.RpcMethodAccess,
        user_id: str | None,
    ) -> shv.SHVType:
        match path, method:
            case ["value", "get"] if access >= shv.RpcMethodAccess.READ:
                return self.value
            case ["value", "set"] if access >= shv.RpcMethodAccess.WRITE:
                old = self.value
                self.value = param
                if old != param:
                    await self._signal("value", value=param)
                return None
            case ["lock", "take"] if access >= shv.RpcMethodAccess.COMMAND:
                if user_id is None:
                    raise shv.RpcUserIDRequiredError
                if self.lock_owner is not None:
                    raise shv.RpcMethodCallExceptionError("Already taken")
                self.lock_owner = user_id
                await self._signal("lock", "owner", value=user_id)
                return None
            case ["lock", "release"] if access >= shv.RpcMethodAccess.READ:
                if user_id is None:
                    raise shv.RpcUserIDRequiredError
                if self.lock_owner != user_id:
                    raise shv.RpcMethodCallExceptionError("Not taken by you")
                self.lock_owner = None
                await self._signal("lock", "owner", value=None)
                return None
            case ["lock", "owner"] if access >= shv.RpcMethodAccess.READ:
                return self.lock_owner
        return await super()._method_call(path, method, param, access, user_id)
