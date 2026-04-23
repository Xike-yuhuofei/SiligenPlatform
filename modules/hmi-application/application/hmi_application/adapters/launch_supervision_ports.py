from __future__ import annotations

from typing import Protocol


class BackendManagerBasic(Protocol):
    def start(self) -> tuple[bool, str]: ...

    def wait_ready(self, timeout: float = 5.0) -> tuple[bool, str]: ...

    def stop(self) -> None: ...


class BackendManagerDetailed(Protocol):
    def start_detailed(self) -> object: ...

    def wait_ready_detailed(self, timeout: float = 5.0) -> object: ...

    def stop(self) -> None: ...


BackendController = BackendManagerBasic | BackendManagerDetailed


class TcpClientLike(Protocol):
    def connect(self, timeout: float = 3.0) -> bool: ...

    def disconnect(self) -> None: ...


class HardwareProtocolLike(Protocol):
    def connect_hardware(
        self,
        card_ip: str = "",
        local_ip: str = "",
        timeout: float = 15.0,
    ) -> tuple[bool, str]: ...


class RuntimeStatusProbeLike(Protocol):
    def get_status_detailed(self, timeout: float = 5.0) -> object: ...


__all__ = [
    "BackendController",
    "BackendManagerBasic",
    "BackendManagerDetailed",
    "HardwareProtocolLike",
    "RuntimeStatusProbeLike",
    "TcpClientLike",
]
