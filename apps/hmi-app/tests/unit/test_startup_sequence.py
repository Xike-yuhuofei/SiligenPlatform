import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client.startup_sequence import normalize_launch_mode, run_launch_sequence


class _FakeBackend:
    def __init__(self, start_result=(True, "started"), wait_result=(True, "ready")) -> None:
        self.start_result = start_result
        self.wait_result = wait_result
        self.start_calls = 0
        self.wait_calls = 0

    def start(self):
        self.start_calls += 1
        return self.start_result

    def wait_ready(self):
        self.wait_calls += 1
        return self.wait_result


class _FakeClient:
    def __init__(self, connect_result=True) -> None:
        self.connect_result = connect_result
        self.connect_calls = 0

    def connect(self):
        self.connect_calls += 1
        return self.connect_result


class _FakeProtocol:
    def __init__(self, hardware_result=(True, "hardware ready")) -> None:
        self.hardware_result = hardware_result
        self.hardware_calls = 0

    def connect_hardware(self):
        self.hardware_calls += 1
        return self.hardware_result


class StartupSequenceContractTest(unittest.TestCase):
    def test_normalize_launch_mode_rejects_unknown_value(self) -> None:
        with self.assertRaisesRegex(ValueError, "Unsupported launch mode"):
            normalize_launch_mode("auto")

    def test_offline_launch_skips_backend_tcp_and_hardware(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient()
        protocol = _FakeProtocol()
        progress_events = []

        result = run_launch_sequence(
            "offline",
            backend,
            client,
            protocol,
            progress_callback=lambda message, percent: progress_events.append((message, percent)),
        )

        self.assertTrue(result.success)
        self.assertEqual(result.requested_mode, "offline")
        self.assertEqual(result.effective_mode, "offline")
        self.assertEqual(result.phase, "offline")
        self.assertFalse(result.backend_started)
        self.assertFalse(result.tcp_connected)
        self.assertFalse(result.hardware_ready)
        self.assertEqual(backend.start_calls, 0)
        self.assertEqual(backend.wait_calls, 0)
        self.assertEqual(client.connect_calls, 0)
        self.assertEqual(protocol.hardware_calls, 0)
        self.assertEqual(progress_events, [("Offline mode: startup skipped", 100)])

    def test_online_launch_reports_backend_failure(self) -> None:
        backend = _FakeBackend(start_result=(False, "missing gateway"))
        client = _FakeClient()
        protocol = _FakeProtocol()

        result = run_launch_sequence("online", backend, client, protocol)

        self.assertFalse(result.success)
        self.assertEqual(result.phase, "backend")
        self.assertIn("missing gateway", result.user_message)
        self.assertEqual(backend.start_calls, 1)
        self.assertEqual(backend.wait_calls, 0)
        self.assertEqual(client.connect_calls, 0)
        self.assertEqual(protocol.hardware_calls, 0)

    def test_online_launch_reports_ready_when_all_stages_succeed(self) -> None:
        backend = _FakeBackend()
        client = _FakeClient(connect_result=True)
        protocol = _FakeProtocol(hardware_result=(True, "ready"))
        progress_events = []

        result = run_launch_sequence(
            "online",
            backend,
            client,
            protocol,
            progress_callback=lambda message, percent: progress_events.append((message, percent)),
        )

        self.assertTrue(result.success)
        self.assertEqual(result.phase, "ready")
        self.assertTrue(result.backend_started)
        self.assertTrue(result.tcp_connected)
        self.assertTrue(result.hardware_ready)
        self.assertEqual(backend.start_calls, 1)
        self.assertEqual(backend.wait_calls, 1)
        self.assertEqual(client.connect_calls, 1)
        self.assertEqual(protocol.hardware_calls, 1)
        self.assertEqual(
            progress_events,
            [
                ("Starting backend...", 10),
                ("Waiting for backend...", 20),
                ("Backend ready", 30),
                ("Connecting TCP...", 40),
                ("TCP connected", 60),
                ("Initializing hardware...", 70),
                ("System ready", 100),
            ],
        )


if __name__ == "__main__":
    unittest.main()
