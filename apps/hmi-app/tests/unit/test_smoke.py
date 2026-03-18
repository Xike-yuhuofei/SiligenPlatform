import py_compile
from pathlib import Path
import unittest


class SmokeTest(unittest.TestCase):
    def test_key_files_are_syntax_valid(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        candidates = [
            project_root / 'src' / 'hmi_client' / 'main.py',
            project_root / 'src' / 'hmi_client' / 'client' / 'tcp_client.py',
            project_root / 'src' / 'hmi_client' / 'client' / 'backend_manager.py',
            project_root / 'src' / 'hmi_client' / 'client' / 'gateway_launch.py',
        ]
        for candidate in candidates:
            self.assertTrue(candidate.exists(), f'missing file: {candidate}')
            py_compile.compile(str(candidate), doraise=True)


if __name__ == '__main__':
    unittest.main()
