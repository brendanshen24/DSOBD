#!/usr/bin/env python3
"""Run a compiled C or Swift test executable against the existing adapter mock."""
import importlib.util
import os
import socket
import subprocess
import sys
import threading
from pathlib import Path

spec = importlib.util.spec_from_file_location("mock", Path(__file__).resolve().parents[1] / "tools/mock_vp01.py")
mock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mock)
mock.FAST = "--fast" in sys.argv
listener = socket.socket()
listener.bind(("127.0.0.1", 0))
listener.listen(1)
port = listener.getsockname()[1]
stats = []
def serve():
    connection, _ = listener.accept()
    with connection:
        session = mock.Session(connection)
        session.run()
        stats.append(session.stats)
worker = threading.Thread(target=serve, daemon=True)
worker.start()
if sys.argv[1] == "--mac":
    env = dict(os.environ, DSOBD_MOCK_PORT=str(port))
    command = ["swift", "test", "--package-path", "macos_app", "--skip-build", "--filter", "DiagnosticsTests.testMockConnection"]
else:
    env = os.environ
    command = [sys.argv[1], str(port)]
result = subprocess.run(command, env=env, timeout=120)
listener.close()
worker.join(2)
assert result.returncode == 0, result.returncode
assert stats and stats[0]["stopped"] == 0, stats
