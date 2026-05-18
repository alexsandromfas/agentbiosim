"""Low-overhead runtime diagnostics for long AgentBioSim runs."""
from __future__ import annotations

import atexit
import faulthandler
import json
import os
import sys
import threading
import time
import traceback
from typing import Any


_LOG_FILE = None
_LOG_PATH: str | None = None
_OLD_SYS_EXCEPTHOOK = None
_OLD_THREADING_EXCEPTHOOK = None
_INSTALLED = False


def _json_safe(value: Any) -> Any:
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    if isinstance(value, (list, tuple)):
        return [_json_safe(v) for v in value]
    if isinstance(value, dict):
        return {str(k): _json_safe(v) for k, v in value.items()}
    return str(value)


def log_event(event: str, **fields: Any) -> None:
    """Append one JSON line to the active runtime log."""
    if _LOG_FILE is None:
        return
    payload = {
        "ts": time.strftime("%Y-%m-%d %H:%M:%S"),
        "event": event,
    }
    payload.update({k: _json_safe(v) for k, v in fields.items()})
    try:
        _LOG_FILE.write(json.dumps(payload, ensure_ascii=True, separators=(",", ":")) + "\n")
        _LOG_FILE.flush()
    except Exception:
        pass


def log_exception(event: str, exc_type, exc, tb) -> None:
    text = "".join(traceback.format_exception(exc_type, exc, tb))
    log_event(event, exception=str(exc), traceback=text[-12000:])


def setup_runtime_diagnostics(root_dir: str | None = None) -> str:
    """Install persistent crash/thread logging and faulthandler.

    Steady-state cost is effectively zero. It writes on process events,
    exceptions, and explicit heartbeat calls.
    """
    global _LOG_FILE, _LOG_PATH, _OLD_SYS_EXCEPTHOOK, _OLD_THREADING_EXCEPTHOOK
    global _INSTALLED

    if _INSTALLED and _LOG_PATH:
        return _LOG_PATH

    root_dir = root_dir or os.getcwd()
    logs_dir = os.path.join(root_dir, "logs", "runtime")
    os.makedirs(logs_dir, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    _LOG_PATH = os.path.join(logs_dir, f"agentbiosim_{stamp}_pid{os.getpid()}.log")
    _LOG_FILE = open(_LOG_PATH, "a", encoding="utf-8", buffering=1)

    try:
        faulthandler.enable(file=_LOG_FILE, all_threads=True)
    except Exception as exc:
        log_event("FAULTHANDLER_ENABLE_FAILED", error=str(exc))

    _OLD_SYS_EXCEPTHOOK = sys.excepthook
    _OLD_THREADING_EXCEPTHOOK = getattr(threading, "excepthook", None)

    def _sys_hook(exc_type, exc, tb):
        log_exception("UNHANDLED_SYS_EXCEPTION", exc_type, exc, tb)
        if _OLD_SYS_EXCEPTHOOK:
            _OLD_SYS_EXCEPTHOOK(exc_type, exc, tb)

    def _thread_hook(args):
        log_exception("UNHANDLED_THREAD_EXCEPTION", args.exc_type, args.exc_value, args.exc_traceback)
        if _OLD_THREADING_EXCEPTHOOK:
            _OLD_THREADING_EXCEPTHOOK(args)

    sys.excepthook = _sys_hook
    threading.excepthook = _thread_hook

    def _on_exit():
        log_event("PROCESS_EXIT")
        try:
            if _LOG_FILE is not None:
                _LOG_FILE.flush()
        except Exception:
            pass

    atexit.register(_on_exit)
    _INSTALLED = True
    log_event("DIAGNOSTICS_STARTED", log_path=_LOG_PATH, python=sys.version, argv=sys.argv)
    return _LOG_PATH


def install_qt_message_handler() -> None:
    """Route Qt warnings/errors to the runtime log if PyQt6 is available."""
    try:
        from PyQt6.QtCore import qInstallMessageHandler
    except Exception:
        return

    def _handler(mode, context, message):
        try:
            log_event(
                "QT_MESSAGE",
                mode=str(mode),
                message=message,
                file=getattr(context, "file", None),
                line=getattr(context, "line", None),
                function=getattr(context, "function", None),
            )
        except Exception:
            pass

    try:
        qInstallMessageHandler(_handler)
    except Exception:
        pass
